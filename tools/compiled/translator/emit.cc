/*********************************************************************
 *
 * Copyright (C) 2008,  Simon Kagstrom
 *
 * Filename:      emit.cc
 * Author:        Simon Kagstrom <simon.kagstrom@gmail.com>
 * Description:   Instruction emitter
 *
 * $Id:$
 *
 ********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include <emit.hh>
#include <controller.hh>

#define do_vsnprintf(buf, fmt) do {\
  va_list ap; \
  int r; \
  assert ( fmt != NULL ); \
  va_start(ap, fmt); \
  r = vsnprintf(buf, 255, fmt, ap); \
  va_end(ap); \
} while(0)


Emit::Emit()
{
  this->fp = stdout;
}

void Emit::bc_pushconst_u(uint32_t val)
{
  if (val >= 0 && val <= 5)
    this->writeIndent("iconst_%u", val);
  else if (val <= 127)
    this->writeIndent("bipush %u", val);
  else if (val <= 32767)
    this->writeIndent("sipush %u", val);
  else
    this->writeIndent("ldc %u", val);
}

void Emit::bc_pushconst(int32_t val)
{
  if (val == -1)
    this->writeIndent("iconst_m1");
  else if (val >= 0 && val <= 5)
    this->writeIndent("iconst_%d", val);
  else if (val >= -128 && val <= 127)
    this->writeIndent("bipush %d", val);
  else if (val >= -32768 && val <= 32767)
    this->writeIndent("sipush %d", val);
  else
    this->writeIndent("ldc %d", val);
}

void Emit::bc_pushaddress(MIPS_register_t reg, int32_t extra)
{
  this->bc_pushregister(reg);
  if (extra != 0)
    {
      this->bc_pushconst(extra);
      this->bc_iadd();
    }
}

void Emit::bc_pushindex(MIPS_register_t reg, int32_t extra)
{
  bool push_extra_before = false;

  this->bc_pushregister(reg);
  if (extra != 0 && extra % 4 != 0)
    {
      this->bc_pushconst(extra);
      this->bc_iadd();
      push_extra_before = true;
    }
  this->bc_pushconst(2);
  this->bc_iushr();
  if ( !push_extra_before && extra != 0 )
    {
      this->bc_pushconst(extra / 4);
      this->bc_iadd();
    }
}

void Emit::bc_load_store_helper(const char *type, int nr)
{
  if (nr >= 0 && nr <=3)
    this->writeIndent("%s_%d", type, nr);
  else
    this->writeIndent("%s %d", type, nr);
}

void Emit::bc_iload(int nr)
{
  this->bc_load_store_helper("iload", nr);
}

void Emit::bc_istore(int nr)
{
  this->bc_load_store_helper("istore", nr);
}

void Emit::bc_pushregister(MIPS_register_t reg)
{
  if (regalloc->regIsStatic(reg))
    this->bc_getstatic( regalloc->regToStatic(reg) );
  else if (reg == R_ZERO || !regalloc->regIsAllocated(reg))
    this->bc_pushconst(0);
  else if (reg == R_MEM)
    this->bc_getstatic( "CRunTime/memory [I" );
  else
    this->bc_iload( regalloc->regToLocal(reg) );
}

void Emit::bc_popregister(MIPS_register_t reg)
{
  if (regalloc->regIsStatic(reg))
    this->bc_putstatic( regalloc->regToStatic(reg) );
  else if (!regalloc->regIsAllocated(reg)) /* This is an error! */
    {
      fprintf(stderr, "Warning/Error at 0x%08x: Popping to register %s, which is not allocated\n",
              controller->getCurrentInstruction()->getAddress(), mips_reg_strings[reg]);
    }
  else
    this->bc_istore( regalloc->regToLocal(reg) );
}

void Emit::bc_iinc(MIPS_register_t reg, int extra)
{
  this->write("\tiinc %d %d", regalloc->regToLocal(reg), extra);
}

void Emit::bc_label(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);
  this->output(buf);
  this->output(":\n");
}

void Emit::bc_condbranch(const char *fmt, ...)
{
  char buf[255];

  this->output("\t");
  do_vsnprintf(buf, fmt);
  this->output(buf);
  this->output("\n");
}

void Emit::bc_generic(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);
  this->output(buf);
}

void Emit::bc_invokestatic(const char *fmt, ...)
{
  char buf[255];

  this->output("\tinvokestatic ");
  do_vsnprintf(buf, fmt);
  this->output(buf);
  this->output("\n");
}

void Emit::bc_lookupswitch(int n, uint32_t *table,
                           const char *def)
{
  this->output("\tlookupswitch\n");

  for (int i = 0; i < n; i++)
    {
      this->write("\t\t0x%x : L_%x",
                  table[i], table[i]);
    }
  this->write("\t\tdefault: %s", def);
}


void Emit::error(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);
  this->output("; ERROR: ");
  this->output(buf);
  fprintf(stderr, "ERROR: %s\n", buf);
}

void Emit::warning(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);

  this->output("; WARNING: ");
  this->output(buf);
  fprintf(stderr, "WARNING: %s\n", buf);
}


void Emit::write(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);

  this->output(buf);
  this->output("\n");
}

void Emit::writeIndent(const char *fmt, ...)
{
  char buf[255];

  do_vsnprintf(buf, fmt);

  this->output("\t");
  this->output(buf);
  this->output("\n");
}

void Emit::output(char *what)
{
  fprintf(this->fp, what);
}

void Emit::setOutputFile(const char *filename)
{
  if (this->fp != stdout)
    fclose(this->fp);
  this->fp = fopen(filename, "w");
  if (!this->fp)
    {
      fprintf(stderr, "Cannot open %s\n", filename);
      exit(1);
    }
}

Emit *emit;
