/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Generate CPU-specific bit-size and similar #defines.
 */
#include <stdio.h>

#if defined(sgi)
#ifndef IRIX
# error "IRIX is not defined"
#endif
#endif

#if defined(__sun)
#if defined(__svr4) || defined(__svr4__)
#ifndef SOLARIS
# error "SOLARIS is not defined"
#endif
#else
#ifndef SUNOS4
# error "SUNOS4 is not defined"
#endif
#endif
#endif

#if defined(__hpux)
#ifndef HPUX
# error "HPUX is not defined"
#endif
#endif

#if defined(__alpha)
#ifndef OSF1
# error "OSF1 is not defined"
#endif
#endif

#if defined(_IBMR2)
#ifndef AIX
# error "AIX is not defined"
#endif
#endif

#if defined(linux)
#ifndef LINUX
# error "LINUX is not defined"
#endif
#endif

#if defined(bsdi)
#ifndef BSDI
# error "BSDI is not defined"
#endif
#endif

#if defined(M_UNIX)
#ifndef SCO
# error "SCO is not defined"
#endif
#endif

#if !defined(M_UNIX) && defined(_USLC_)
#ifndef UNIXWARE
# error "UNIXWARE is not defined"
#endif
#endif

#ifdef __MWERKS__
#define XP_MAC 1
#endif

/************************************************************************/

/* Generate cpucfg.h */
#ifdef XP_MAC
#include <Types.h>
#define INT64	UnsignedWide
#else
#ifdef XP_PC
#ifdef WIN32
#define INT64	_int64
#else
#define INT64	long
#endif
#else
#if defined(HPUX) || defined(SCO) || defined(UNIXWARE)
#define INT64	long
#else
#define INT64	long long
#endif
#endif
#endif

typedef void *prword;

struct align_short {
    char c;
    short a;
};
struct align_int {
    char c;
    int a;
};
struct align_long {
    char c;
    long a;
};
struct align_int64 {
    char c;
    INT64 a;
};
struct align_fakelonglong {
    char c;
    struct {
	long hi, lo;
    } a;
};
struct align_float {
    char c;
    float a;
};
struct align_double {
    char c;
    double a;
};
struct align_pointer {
    char c;
    void *a;
};
struct align_prword {
    char c;
    prword a;
};

#define ALIGN_OF(type) \
    (((char*)&(((struct align_##type *)0)->a)) - ((char*)0))

int bpb;

static int Log2(int n)
{
    int log2 = 0;

    if (n & (n-1))
	log2++;
    if (n >> 16)
	log2 += 16, n >>= 16;
    if (n >> 8)
	log2 += 8, n >>= 8;
    if (n >> 4)
	log2 += 4, n >>= 4;
    if (n >> 2)
	log2 += 2, n >>= 2;
    if (n >> 1)
	log2++;
    return log2;
}

/* We assume that int's are 32 bits */
static void do64(void)
{
    union {
	long i;
	char c[4];
    } u;

    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
	printf("#undef  IS_LITTLE_ENDIAN\n");
	printf("#define IS_BIG_ENDIAN 1\n\n");
    } else {
	printf("#define IS_LITTLE_ENDIAN 1\n");
	printf("#undef  IS_BIG_ENDIAN\n\n");
    }
}

static void do32(void)
{
    union {
	long i;
	char c[4];
    } u;

    u.i = 0x01020304;
    if (u.c[0] == 0x01) {
	printf("#undef  IS_LITTLE_ENDIAN\n");
	printf("#define IS_BIG_ENDIAN 1\n\n");
    } else {
	printf("#define IS_LITTLE_ENDIAN 1\n");
	printf("#undef  IS_BIG_ENDIAN\n\n");
    }
}

/*
 * Conceivably this could actually be used, but there is lots of code out
 * there with ands and shifts in it that assumes a byte is exactly 8 bits,
 * so forget about porting THIS code to all those non 8 bit byte machines.
 */
static void BitsPerByte(void)
{
    bpb = 8;
}

int main(int argc, char **argv)
{
    BitsPerByte();

    printf("#ifndef nspr_cpucfg___\n");
    printf("#define nspr_cpucfg___\n\n");

    printf("/* AUTOMATICALLY GENERATED - DO NOT EDIT */\n\n");

    if (sizeof(long) == 8) {
	do64();
    } else {
	do32();
    }
    printf("#define PR_BYTES_PER_BYTE   %dL\n", sizeof(char));
    printf("#define PR_BYTES_PER_SHORT  %dL\n", sizeof(short));
    printf("#define PR_BYTES_PER_INT    %dL\n", sizeof(int));
    printf("#define PR_BYTES_PER_INT64  %dL\n", 8);
    printf("#define PR_BYTES_PER_LONG   %dL\n", sizeof(long));
    printf("#define PR_BYTES_PER_FLOAT  %dL\n", sizeof(float));
    printf("#define PR_BYTES_PER_DOUBLE %dL\n", sizeof(double));
    printf("#define PR_BYTES_PER_WORD   %dL\n", sizeof(prword));
    printf("#define PR_BYTES_PER_DWORD  %dL\n", 8);
    printf("\n");

    printf("#define PR_BITS_PER_BYTE    %dL\n", bpb);
    printf("#define PR_BITS_PER_SHORT   %dL\n", bpb * sizeof(short));
    printf("#define PR_BITS_PER_INT     %dL\n", bpb * sizeof(int));
    printf("#define PR_BITS_PER_INT64   %dL\n", bpb * 8);
    printf("#define PR_BITS_PER_LONG    %dL\n", bpb * sizeof(long));
    printf("#define PR_BITS_PER_FLOAT   %dL\n", bpb * sizeof(float));
    printf("#define PR_BITS_PER_DOUBLE  %dL\n", bpb * sizeof(double));
    printf("#define PR_BITS_PER_WORD    %dL\n", bpb * sizeof(prword));
    printf("\n");

    printf("#define PR_BITS_PER_BYTE_LOG2   %dL\n", Log2(bpb));
    printf("#define PR_BITS_PER_SHORT_LOG2  %dL\n", Log2(bpb * sizeof(short)));
    printf("#define PR_BITS_PER_INT_LOG2    %dL\n", Log2(bpb * sizeof(int)));
    printf("#define PR_BITS_PER_INT64_LOG2  %dL\n", 6);
    printf("#define PR_BITS_PER_LONG_LOG2   %dL\n", Log2(bpb * sizeof(long)));
    printf("#define PR_BITS_PER_FLOAT_LOG2  %dL\n", Log2(bpb * sizeof(float)));
    printf("#define PR_BITS_PER_DOUBLE_LOG2 %dL\n", Log2(bpb * sizeof(double)));
    printf("#define PR_BITS_PER_WORD_LOG2   %dL\n", Log2(bpb * sizeof(prword)));
    printf("\n");

    printf("#define PR_ALIGN_OF_SHORT   %dL\n", ALIGN_OF(short));
    printf("#define PR_ALIGN_OF_INT     %dL\n", ALIGN_OF(int));
    printf("#define PR_ALIGN_OF_LONG    %dL\n", ALIGN_OF(long));
    if (sizeof(INT64) < 8) {
	/* this machine doesn't actually support int64's */
	printf("#define PR_ALIGN_OF_INT64   %dL\n", ALIGN_OF(fakelonglong));
    } else {
	printf("#define PR_ALIGN_OF_INT64   %dL\n", ALIGN_OF(int64));
    }
    printf("#define PR_ALIGN_OF_FLOAT   %dL\n", ALIGN_OF(float));
    printf("#define PR_ALIGN_OF_DOUBLE  %dL\n", ALIGN_OF(double));
    printf("#define PR_ALIGN_OF_POINTER %dL\n", ALIGN_OF(pointer));
    printf("#define PR_ALIGN_OF_WORD    %dL\n", ALIGN_OF(prword));
    printf("\n");

    printf("#define PR_BYTES_PER_WORD_LOG2   %dL\n", Log2(sizeof(prword)));
    printf("#define PR_BYTES_PER_DWORD_LOG2  %dL\n", Log2(8));
    printf("#define PR_WORDS_PER_DWORD_LOG2  %dL\n", Log2(8/sizeof(prword)));
    printf("\n");

    printf("#endif /* nspr_cpucfg___ */\n");

    return 0;
}
