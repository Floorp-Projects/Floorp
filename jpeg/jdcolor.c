/*
 * jdcolor.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains output colorspace conversion routines.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jconfig.h"


/* Private subobject */

typedef struct {
  struct jpeg_color_deconverter pub; /* public fields */

  /* These fields are not needed anymore as these are now static tables */

#if 0
  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;		/* => table for Cr to R conversion */
  int * Cb_b_tab;		/* => table for Cb to B conversion */
  INT32 * Cr_g_tab;		/* => table for Cr to G conversion */
  INT32 * Cb_g_tab;		/* => table for Cb to G conversion */
#endif
} my_color_deconverter;

typedef my_color_deconverter * my_cconvert_ptr;


/**************** YCbCr -> RGB conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *	R = Y                + 1.40200 * Cr
 *	G = Y - 0.34414 * Cb - 0.71414 * Cr
 *	B = Y + 1.77200 * Cb
 * where Cb and Cr represent the incoming values less CENTERJSAMPLE.
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 * Notice that Y, being an integral input, does not contribute any fraction
 * so it need not participate in the rounding.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times Cb and Cr for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The Cr=>R and Cb=>B values can be rounded to integers in advance; the
 * values for the G calculation are left scaled up, since we must add them
 * together before rounding.
 */

#define SCALEBITS	16	/* speediest right-shift on some machines */
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

/* Use static tables for color processing. */

const int Cr_r_tab[(MAXJSAMPLE+1) * SIZEOF(int)] ={
  0xffffff4dUL, 0xffffff4eUL, 0xffffff4fUL, 0xffffff51UL, 0xffffff52UL, 0xffffff54UL, 
  0xffffff55UL, 0xffffff56UL, 0xffffff58UL, 0xffffff59UL, 0xffffff5bUL, 0xffffff5cUL, 
  0xffffff5dUL, 0xffffff5fUL, 0xffffff60UL, 0xffffff62UL, 0xffffff63UL, 0xffffff64UL, 
  0xffffff66UL, 0xffffff67UL, 0xffffff69UL, 0xffffff6aUL, 0xffffff6bUL, 0xffffff6dUL, 
  0xffffff6eUL, 0xffffff70UL, 0xffffff71UL, 0xffffff72UL, 0xffffff74UL, 0xffffff75UL, 
  0xffffff77UL, 0xffffff78UL, 0xffffff79UL, 0xffffff7bUL, 0xffffff7cUL, 0xffffff7eUL, 
  0xffffff7fUL, 0xffffff80UL, 0xffffff82UL, 0xffffff83UL, 0xffffff85UL, 0xffffff86UL, 
  0xffffff87UL, 0xffffff89UL, 0xffffff8aUL, 0xffffff8cUL, 0xffffff8dUL, 0xffffff8eUL, 
  0xffffff90UL, 0xffffff91UL, 0xffffff93UL, 0xffffff94UL, 0xffffff95UL, 0xffffff97UL, 
  0xffffff98UL, 0xffffff9aUL, 0xffffff9bUL, 0xffffff9cUL, 0xffffff9eUL, 0xffffff9fUL, 
  0xffffffa1UL, 0xffffffa2UL, 0xffffffa3UL, 0xffffffa5UL, 0xffffffa6UL, 0xffffffa8UL, 
  0xffffffa9UL, 0xffffffaaUL, 0xffffffacUL, 0xffffffadUL, 0xffffffafUL, 0xffffffb0UL, 
  0xffffffb1UL, 0xffffffb3UL, 0xffffffb4UL, 0xffffffb6UL, 0xffffffb7UL, 0xffffffb8UL, 
  0xffffffbaUL, 0xffffffbbUL, 0xffffffbdUL, 0xffffffbeUL, 0xffffffc0UL, 0xffffffc1UL, 
  0xffffffc2UL, 0xffffffc4UL, 0xffffffc5UL, 0xffffffc7UL, 0xffffffc8UL, 0xffffffc9UL, 
  0xffffffcbUL, 0xffffffccUL, 0xffffffceUL, 0xffffffcfUL, 0xffffffd0UL, 0xffffffd2UL, 
  0xffffffd3UL, 0xffffffd5UL, 0xffffffd6UL, 0xffffffd7UL, 0xffffffd9UL, 0xffffffdaUL, 
  0xffffffdcUL, 0xffffffddUL, 0xffffffdeUL, 0xffffffe0UL, 0xffffffe1UL, 0xffffffe3UL, 
  0xffffffe4UL, 0xffffffe5UL, 0xffffffe7UL, 0xffffffe8UL, 0xffffffeaUL, 0xffffffebUL, 
  0xffffffecUL, 0xffffffeeUL, 0xffffffefUL, 0xfffffff1UL, 0xfffffff2UL, 0xfffffff3UL, 
  0xfffffff5UL, 0xfffffff6UL, 0xfffffff8UL, 0xfffffff9UL, 0xfffffffaUL, 0xfffffffcUL, 
  0xfffffffdUL, 0xffffffffUL,       0x00UL,       0x01UL,       0x03UL,       0x04UL, 
        0x06UL,       0x07UL,       0x08UL,       0x0aUL,       0x0bUL,       0x0dUL, 
        0x0eUL,       0x0fUL,       0x11UL,       0x12UL,       0x14UL,       0x15UL, 
        0x16UL,       0x18UL,       0x19UL,       0x1bUL,       0x1cUL,       0x1dUL, 
        0x1fUL,       0x20UL,       0x22UL,       0x23UL,       0x24UL,       0x26UL, 
        0x27UL,       0x29UL,       0x2aUL,       0x2bUL,       0x2dUL,       0x2eUL, 
        0x30UL,       0x31UL,       0x32UL,       0x34UL,       0x35UL,       0x37UL, 
        0x38UL,       0x39UL,       0x3bUL,       0x3cUL,       0x3eUL,       0x3fUL, 
        0x40UL,       0x42UL,       0x43UL,       0x45UL,       0x46UL,       0x48UL, 
        0x49UL,       0x4aUL,       0x4cUL,       0x4dUL,       0x4fUL,       0x50UL, 
        0x51UL,       0x53UL,       0x54UL,       0x56UL,       0x57UL,       0x58UL, 
        0x5aUL,       0x5bUL,       0x5dUL,       0x5eUL,       0x5fUL,       0x61UL, 
        0x62UL,       0x64UL,       0x65UL,       0x66UL,       0x68UL,       0x69UL, 
        0x6bUL,       0x6cUL,       0x6dUL,       0x6fUL,       0x70UL,       0x72UL, 
        0x73UL,       0x74UL,       0x76UL,       0x77UL,       0x79UL,       0x7aUL, 
        0x7bUL,       0x7dUL,       0x7eUL,       0x80UL,       0x81UL,       0x82UL, 
        0x84UL,       0x85UL,       0x87UL,       0x88UL,       0x89UL,       0x8bUL, 
        0x8cUL,       0x8eUL,       0x8fUL,       0x90UL,       0x92UL,       0x93UL, 
        0x95UL,       0x96UL,       0x97UL,       0x99UL,       0x9aUL,       0x9cUL, 
        0x9dUL,       0x9eUL,       0xa0UL,       0xa1UL,       0xa3UL,       0xa4UL, 
        0xa5UL,       0xa7UL,       0xa8UL,       0xaaUL,       0xabUL,       0xacUL, 
        0xaeUL,       0xafUL,       0xb1UL,       0xb2UL
  };

const int Cb_b_tab[(MAXJSAMPLE+1) * SIZEOF(int)] ={
  0xffffff1dUL, 0xffffff1fUL, 0xffffff21UL, 0xffffff22UL, 0xffffff24UL, 0xffffff26UL, 
  0xffffff28UL, 0xffffff2aUL, 0xffffff2bUL, 0xffffff2dUL, 0xffffff2fUL, 0xffffff31UL, 
  0xffffff32UL, 0xffffff34UL, 0xffffff36UL, 0xffffff38UL, 0xffffff3aUL, 0xffffff3bUL, 
  0xffffff3dUL, 0xffffff3fUL, 0xffffff41UL, 0xffffff42UL, 0xffffff44UL, 0xffffff46UL, 
  0xffffff48UL, 0xffffff49UL, 0xffffff4bUL, 0xffffff4dUL, 0xffffff4fUL, 0xffffff51UL, 
  0xffffff52UL, 0xffffff54UL, 0xffffff56UL, 0xffffff58UL, 0xffffff59UL, 0xffffff5bUL, 
  0xffffff5dUL, 0xffffff5fUL, 0xffffff61UL, 0xffffff62UL, 0xffffff64UL, 0xffffff66UL, 
  0xffffff68UL, 0xffffff69UL, 0xffffff6bUL, 0xffffff6dUL, 0xffffff6fUL, 0xffffff70UL, 
  0xffffff72UL, 0xffffff74UL, 0xffffff76UL, 0xffffff78UL, 0xffffff79UL, 0xffffff7bUL, 
  0xffffff7dUL, 0xffffff7fUL, 0xffffff80UL, 0xffffff82UL, 0xffffff84UL, 0xffffff86UL, 
  0xffffff88UL, 0xffffff89UL, 0xffffff8bUL, 0xffffff8dUL, 0xffffff8fUL, 0xffffff90UL, 
  0xffffff92UL, 0xffffff94UL, 0xffffff96UL, 0xffffff97UL, 0xffffff99UL, 0xffffff9bUL, 
  0xffffff9dUL, 0xffffff9fUL, 0xffffffa0UL, 0xffffffa2UL, 0xffffffa4UL, 0xffffffa6UL, 
  0xffffffa7UL, 0xffffffa9UL, 0xffffffabUL, 0xffffffadUL, 0xffffffaeUL, 0xffffffb0UL, 
  0xffffffb2UL, 0xffffffb4UL, 0xffffffb6UL, 0xffffffb7UL, 0xffffffb9UL, 0xffffffbbUL, 
  0xffffffbdUL, 0xffffffbeUL, 0xffffffc0UL, 0xffffffc2UL, 0xffffffc4UL, 0xffffffc6UL, 
  0xffffffc7UL, 0xffffffc9UL, 0xffffffcbUL, 0xffffffcdUL, 0xffffffceUL, 0xffffffd0UL, 
  0xffffffd2UL, 0xffffffd4UL, 0xffffffd5UL, 0xffffffd7UL, 0xffffffd9UL, 0xffffffdbUL, 
  0xffffffddUL, 0xffffffdeUL, 0xffffffe0UL, 0xffffffe2UL, 0xffffffe4UL, 0xffffffe5UL, 
  0xffffffe7UL, 0xffffffe9UL, 0xffffffebUL, 0xffffffedUL, 0xffffffeeUL, 0xfffffff0UL, 
  0xfffffff2UL, 0xfffffff4UL, 0xfffffff5UL, 0xfffffff7UL, 0xfffffff9UL, 0xfffffffbUL, 
  0xfffffffcUL, 0xfffffffeUL,       0x00UL,       0x02UL,       0x04UL,       0x05UL, 
        0x07UL,       0x09UL,       0x0bUL,       0x0cUL,       0x0eUL,       0x10UL, 
        0x12UL,       0x13UL,       0x15UL,       0x17UL,       0x19UL,       0x1bUL, 
        0x1cUL,       0x1eUL,       0x20UL,       0x22UL,       0x23UL,       0x25UL, 
        0x27UL,       0x29UL,       0x2bUL,       0x2cUL,       0x2eUL,       0x30UL, 
        0x32UL,       0x33UL,       0x35UL,       0x37UL,       0x39UL,       0x3aUL, 
        0x3cUL,       0x3eUL,       0x40UL,       0x42UL,       0x43UL,       0x45UL, 
        0x47UL,       0x49UL,       0x4aUL,       0x4cUL,       0x4eUL,       0x50UL, 
        0x52UL,       0x53UL,       0x55UL,       0x57UL,       0x59UL,       0x5aUL, 
        0x5cUL,       0x5eUL,       0x60UL,       0x61UL,       0x63UL,       0x65UL, 
        0x67UL,       0x69UL,       0x6aUL,       0x6cUL,       0x6eUL,       0x70UL, 
        0x71UL,       0x73UL,       0x75UL,       0x77UL,       0x78UL,       0x7aUL, 
        0x7cUL,       0x7eUL,       0x80UL,       0x81UL,       0x83UL,       0x85UL, 
        0x87UL,       0x88UL,       0x8aUL,       0x8cUL,       0x8eUL,       0x90UL, 
        0x91UL,       0x93UL,       0x95UL,       0x97UL,       0x98UL,       0x9aUL, 
        0x9cUL,       0x9eUL,       0x9fUL,       0xa1UL,       0xa3UL,       0xa5UL, 
        0xa7UL,       0xa8UL,       0xaaUL,       0xacUL,       0xaeUL,       0xafUL, 
        0xb1UL,       0xb3UL,       0xb5UL,       0xb7UL,       0xb8UL,       0xbaUL, 
        0xbcUL,       0xbeUL,       0xbfUL,       0xc1UL,       0xc3UL,       0xc5UL, 
        0xc6UL,       0xc8UL,       0xcaUL,       0xccUL,       0xceUL,       0xcfUL, 
        0xd1UL,       0xd3UL,       0xd5UL,       0xd6UL,       0xd8UL,       0xdaUL, 
        0xdcUL,       0xdeUL,       0xdfUL,       0xe1UL
  };

const int Cr_g_tab[(MAXJSAMPLE+1) * SIZEOF(int)] ={
    0x5b6900UL,   0x5ab22eUL,   0x59fb5cUL,   0x59448aUL,   0x588db8UL,   0x57d6e6UL, 
    0x572014UL,   0x566942UL,   0x55b270UL,   0x54fb9eUL,   0x5444ccUL,   0x538dfaUL, 
    0x52d728UL,   0x522056UL,   0x516984UL,   0x50b2b2UL,   0x4ffbe0UL,   0x4f450eUL, 
    0x4e8e3cUL,   0x4dd76aUL,   0x4d2098UL,   0x4c69c6UL,   0x4bb2f4UL,   0x4afc22UL, 
    0x4a4550UL,   0x498e7eUL,   0x48d7acUL,   0x4820daUL,   0x476a08UL,   0x46b336UL, 
    0x45fc64UL,   0x454592UL,   0x448ec0UL,   0x43d7eeUL,   0x43211cUL,   0x426a4aUL, 
    0x41b378UL,   0x40fca6UL,   0x4045d4UL,   0x3f8f02UL,   0x3ed830UL,   0x3e215eUL, 
    0x3d6a8cUL,   0x3cb3baUL,   0x3bfce8UL,   0x3b4616UL,   0x3a8f44UL,   0x39d872UL, 
    0x3921a0UL,   0x386aceUL,   0x37b3fcUL,   0x36fd2aUL,   0x364658UL,   0x358f86UL, 
    0x34d8b4UL,   0x3421e2UL,   0x336b10UL,   0x32b43eUL,   0x31fd6cUL,   0x31469aUL, 
    0x308fc8UL,   0x2fd8f6UL,   0x2f2224UL,   0x2e6b52UL,   0x2db480UL,   0x2cfdaeUL, 
    0x2c46dcUL,   0x2b900aUL,   0x2ad938UL,   0x2a2266UL,   0x296b94UL,   0x28b4c2UL, 
    0x27fdf0UL,   0x27471eUL,   0x26904cUL,   0x25d97aUL,   0x2522a8UL,   0x246bd6UL, 
    0x23b504UL,   0x22fe32UL,   0x224760UL,   0x21908eUL,   0x20d9bcUL,   0x2022eaUL, 
    0x1f6c18UL,   0x1eb546UL,   0x1dfe74UL,   0x1d47a2UL,   0x1c90d0UL,   0x1bd9feUL, 
    0x1b232cUL,   0x1a6c5aUL,   0x19b588UL,   0x18feb6UL,   0x1847e4UL,   0x179112UL, 
    0x16da40UL,   0x16236eUL,   0x156c9cUL,   0x14b5caUL,   0x13fef8UL,   0x134826UL, 
    0x129154UL,   0x11da82UL,   0x1123b0UL,   0x106cdeUL,    0xfb60cUL,    0xeff3aUL, 
     0xe4868UL,    0xd9196UL,    0xcdac4UL,    0xc23f2UL,    0xb6d20UL,    0xab64eUL, 
     0x9ff7cUL,    0x948aaUL,    0x891d8UL,    0x7db06UL,    0x72434UL,    0x66d62UL, 
     0x5b690UL,    0x4ffbeUL,    0x448ecUL,    0x3921aUL,    0x2db48UL,    0x22476UL, 
     0x16da4UL,     0xb6d2UL,        0x0UL, 0xffff492eUL, 0xfffe925cUL, 0xfffddb8aUL, 
  0xfffd24b8UL, 0xfffc6de6UL, 0xfffbb714UL, 0xfffb0042UL, 0xfffa4970UL, 0xfff9929eUL, 
  0xfff8dbccUL, 0xfff824faUL, 0xfff76e28UL, 0xfff6b756UL, 0xfff60084UL, 0xfff549b2UL, 
  0xfff492e0UL, 0xfff3dc0eUL, 0xfff3253cUL, 0xfff26e6aUL, 0xfff1b798UL, 0xfff100c6UL, 
  0xfff049f4UL, 0xffef9322UL, 0xffeedc50UL, 0xffee257eUL, 0xffed6eacUL, 0xffecb7daUL, 
  0xffec0108UL, 0xffeb4a36UL, 0xffea9364UL, 0xffe9dc92UL, 0xffe925c0UL, 0xffe86eeeUL, 
  0xffe7b81cUL, 0xffe7014aUL, 0xffe64a78UL, 0xffe593a6UL, 0xffe4dcd4UL, 0xffe42602UL, 
  0xffe36f30UL, 0xffe2b85eUL, 0xffe2018cUL, 0xffe14abaUL, 0xffe093e8UL, 0xffdfdd16UL, 
  0xffdf2644UL, 0xffde6f72UL, 0xffddb8a0UL, 0xffdd01ceUL, 0xffdc4afcUL, 0xffdb942aUL, 
  0xffdadd58UL, 0xffda2686UL, 0xffd96fb4UL, 0xffd8b8e2UL, 0xffd80210UL, 0xffd74b3eUL, 
  0xffd6946cUL, 0xffd5dd9aUL, 0xffd526c8UL, 0xffd46ff6UL, 0xffd3b924UL, 0xffd30252UL, 
  0xffd24b80UL, 0xffd194aeUL, 0xffd0dddcUL, 0xffd0270aUL, 0xffcf7038UL, 0xffceb966UL, 
  0xffce0294UL, 0xffcd4bc2UL, 0xffcc94f0UL, 0xffcbde1eUL, 0xffcb274cUL, 0xffca707aUL, 
  0xffc9b9a8UL, 0xffc902d6UL, 0xffc84c04UL, 0xffc79532UL, 0xffc6de60UL, 0xffc6278eUL, 
  0xffc570bcUL, 0xffc4b9eaUL, 0xffc40318UL, 0xffc34c46UL, 0xffc29574UL, 0xffc1dea2UL, 
  0xffc127d0UL, 0xffc070feUL, 0xffbfba2cUL, 0xffbf035aUL, 0xffbe4c88UL, 0xffbd95b6UL, 
  0xffbcdee4UL, 0xffbc2812UL, 0xffbb7140UL, 0xffbaba6eUL, 0xffba039cUL, 0xffb94ccaUL, 
  0xffb895f8UL, 0xffb7df26UL, 0xffb72854UL, 0xffb67182UL, 0xffb5bab0UL, 0xffb503deUL, 
  0xffb44d0cUL, 0xffb3963aUL, 0xffb2df68UL, 0xffb22896UL, 0xffb171c4UL, 0xffb0baf2UL, 
  0xffb00420UL, 0xffaf4d4eUL, 0xffae967cUL, 0xffaddfaaUL, 0xffad28d8UL, 0xffac7206UL, 
  0xffabbb34UL, 0xffab0462UL, 0xffaa4d90UL, 0xffa996beUL, 0xffa8dfecUL, 0xffa8291aUL, 
  0xffa77248UL, 0xffa6bb76UL, 0xffa604a4UL, 0xffa54dd2UL
 };

const int Cb_g_tab[(MAXJSAMPLE+1) * SIZEOF(int)] ={
    0x2c8d00UL,   0x2c34e6UL,   0x2bdcccUL,   0x2b84b2UL,   0x2b2c98UL,   0x2ad47eUL, 
    0x2a7c64UL,   0x2a244aUL,   0x29cc30UL,   0x297416UL,   0x291bfcUL,   0x28c3e2UL, 
    0x286bc8UL,   0x2813aeUL,   0x27bb94UL,   0x27637aUL,   0x270b60UL,   0x26b346UL, 
    0x265b2cUL,   0x260312UL,   0x25aaf8UL,   0x2552deUL,   0x24fac4UL,   0x24a2aaUL, 
    0x244a90UL,   0x23f276UL,   0x239a5cUL,   0x234242UL,   0x22ea28UL,   0x22920eUL, 
    0x2239f4UL,   0x21e1daUL,   0x2189c0UL,   0x2131a6UL,   0x20d98cUL,   0x208172UL, 
    0x202958UL,   0x1fd13eUL,   0x1f7924UL,   0x1f210aUL,   0x1ec8f0UL,   0x1e70d6UL, 
    0x1e18bcUL,   0x1dc0a2UL,   0x1d6888UL,   0x1d106eUL,   0x1cb854UL,   0x1c603aUL, 
    0x1c0820UL,   0x1bb006UL,   0x1b57ecUL,   0x1affd2UL,   0x1aa7b8UL,   0x1a4f9eUL, 
    0x19f784UL,   0x199f6aUL,   0x194750UL,   0x18ef36UL,   0x18971cUL,   0x183f02UL, 
    0x17e6e8UL,   0x178eceUL,   0x1736b4UL,   0x16de9aUL,   0x168680UL,   0x162e66UL, 
    0x15d64cUL,   0x157e32UL,   0x152618UL,   0x14cdfeUL,   0x1475e4UL,   0x141dcaUL, 
    0x13c5b0UL,   0x136d96UL,   0x13157cUL,   0x12bd62UL,   0x126548UL,   0x120d2eUL, 
    0x11b514UL,   0x115cfaUL,   0x1104e0UL,   0x10acc6UL,   0x1054acUL,    0xffc92UL, 
     0xfa478UL,    0xf4c5eUL,    0xef444UL,    0xe9c2aUL,    0xe4410UL,    0xdebf6UL, 
     0xd93dcUL,    0xd3bc2UL,    0xce3a8UL,    0xc8b8eUL,    0xc3374UL,    0xbdb5aUL, 
     0xb8340UL,    0xb2b26UL,    0xad30cUL,    0xa7af2UL,    0xa22d8UL,    0x9cabeUL, 
     0x972a4UL,    0x91a8aUL,    0x8c270UL,    0x86a56UL,    0x8123cUL,    0x7ba22UL, 
     0x76208UL,    0x709eeUL,    0x6b1d4UL,    0x659baUL,    0x601a0UL,    0x5a986UL, 
     0x5516cUL,    0x4f952UL,    0x4a138UL,    0x4491eUL,    0x3f104UL,    0x398eaUL, 
     0x340d0UL,    0x2e8b6UL,    0x2909cUL,    0x23882UL,    0x1e068UL,    0x1884eUL, 
     0x13034UL,     0xd81aUL,     0x8000UL,     0x27e6UL, 0xffffcfccUL, 0xffff77b2UL,
  0xffff1f98UL, 0xfffec77eUL, 0xfffe6f64UL, 0xfffe174aUL, 0xfffdbf30UL, 0xfffd6716UL,
  0xfffd0efcUL, 0xfffcb6e2UL, 0xfffc5ec8UL, 0xfffc06aeUL, 0xfffbae94UL, 0xfffb567aUL,
  0xfffafe60UL, 0xfffaa646UL, 0xfffa4e2cUL, 0xfff9f612UL, 0xfff99df8UL, 0xfff945deUL,
  0xfff8edc4UL, 0xfff895aaUL, 0xfff83d90UL, 0xfff7e576UL, 0xfff78d5cUL, 0xfff73542UL,
  0xfff6dd28UL, 0xfff6850eUL, 0xfff62cf4UL, 0xfff5d4daUL, 0xfff57cc0UL, 0xfff524a6UL,
  0xfff4cc8cUL, 0xfff47472UL, 0xfff41c58UL, 0xfff3c43eUL, 0xfff36c24UL, 0xfff3140aUL,
  0xfff2bbf0UL, 0xfff263d6UL, 0xfff20bbcUL, 0xfff1b3a2UL, 0xfff15b88UL, 0xfff1036eUL,
  0xfff0ab54UL, 0xfff0533aUL, 0xffeffb20UL, 0xffefa306UL, 0xffef4aecUL, 0xffeef2d2UL,
  0xffee9ab8UL, 0xffee429eUL, 0xffedea84UL, 0xffed926aUL, 0xffed3a50UL, 0xffece236UL,
  0xffec8a1cUL, 0xffec3202UL, 0xffebd9e8UL, 0xffeb81ceUL, 0xffeb29b4UL, 0xffead19aUL,
  0xffea7980UL, 0xffea2166UL, 0xffe9c94cUL, 0xffe97132UL, 0xffe91918UL, 0xffe8c0feUL,
  0xffe868e4UL, 0xffe810caUL, 0xffe7b8b0UL, 0xffe76096UL, 0xffe7087cUL, 0xffe6b062UL,
  0xffe65848UL, 0xffe6002eUL, 0xffe5a814UL, 0xffe54ffaUL, 0xffe4f7e0UL, 0xffe49fc6UL,
  0xffe447acUL, 0xffe3ef92UL, 0xffe39778UL, 0xffe33f5eUL, 0xffe2e744UL, 0xffe28f2aUL,
  0xffe23710UL, 0xffe1def6UL, 0xffe186dcUL, 0xffe12ec2UL, 0xffe0d6a8UL, 0xffe07e8eUL,
  0xffe02674UL, 0xffdfce5aUL, 0xffdf7640UL, 0xffdf1e26UL, 0xffdec60cUL, 0xffde6df2UL,
  0xffde15d8UL, 0xffddbdbeUL, 0xffdd65a4UL, 0xffdd0d8aUL, 0xffdcb570UL, 0xffdc5d56UL,
  0xffdc053cUL, 0xffdbad22UL, 0xffdb5508UL, 0xffdafceeUL, 0xffdaa4d4UL, 0xffda4cbaUL,
  0xffd9f4a0UL, 0xffd99c86UL, 0xffd9446cUL, 0xffd8ec52UL, 0xffd89438UL, 0xffd83c1eUL,
  0xffd7e404UL, 0xffd78beaUL, 0xffd733d0UL, 0xffd6dbb6UL, 0xffd6839cUL, 0xffd62b82UL,
  0xffd5d368UL, 0xffd57b4eUL, 0xffd52334UL, 0xffd4cb1aUL
 };

/*
 * Initialize tables for YCC->RGB colorspace conversion.
 */

LOCAL(void)
build_ycc_rgb_table (j_decompress_ptr cinfo)
{

  /* The code below was used to generate the static tables above */

#if 0
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int i;
  INT32 x;
  SHIFT_TEMPS

  cconvert->Cr_r_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  cconvert->Cb_b_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  cconvert->Cr_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));
  cconvert->Cb_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));

  for (i = 0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    cconvert->Cr_r_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    cconvert->Cb_b_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    cconvert->Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    cconvert->Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
  }
#endif /* 0 */
}


/*
 * Convert some rows of samples to the output colorspace.
 *
 * Note that we change from noninterleaved, one-plane-per-component format
 * to interleaved-pixel format.  The output buffer is therefore three times
 * as wide as the input buffer.
 * A starting row offset is provided only for the input buffer.  The caller
 * can easily adjust the passed output_buf value to accommodate any row
 * offset required on that side.
 */

METHODDEF(void)
ycc_rgb_convert (j_decompress_ptr cinfo,
		 JSAMPIMAGE input_buf, JDIMENSION input_row,
		 JSAMPARRAY output_buf, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  JSAMPLE * range_limit_y;
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  JSAMPLE * range_limit = cinfo->sample_range_limit;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      range_limit_y = range_limit + y;
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit_y[Cr_r_tab[cr]];
      outptr[RGB_GREEN] = range_limit_y[
			      ((int) RIGHT_SHIFT(Cb_g_tab[cb] + Cr_g_tab[cr],
						 SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit_y[Cb_b_tab[cb]];
      outptr += RGB_PIXELSIZE;
    }
  }
}


/**************** Cases other than YCbCr -> RGB **************/


/*
 * Color conversion for no colorspace change: just copy the data,
 * converting from separate-planes to interleaved representation.
 */

METHODDEF(void)
null_convert (j_decompress_ptr cinfo,
	      JSAMPIMAGE input_buf, JDIMENSION input_row,
	      JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION count;
  register int num_components = cinfo->num_components;
  JDIMENSION num_cols = cinfo->output_width;
  int ci;

  while (--num_rows >= 0) {
    for (ci = 0; ci < num_components; ci++) {
      inptr = input_buf[ci][input_row];
      outptr = output_buf[0] + ci;
      for (count = num_cols; count > 0; count--) {
	*outptr = *inptr++;	/* needn't bother with GETJSAMPLE() here */
	outptr += num_components;
      }
    }
    input_row++;
    output_buf++;
  }
}


/*
 * Color conversion for grayscale: just copy the data.
 * This also works for YCbCr -> grayscale conversion, in which
 * we just copy the Y (luminance) component and ignore chrominance.
 */

METHODDEF(void)
grayscale_convert (j_decompress_ptr cinfo,
		   JSAMPIMAGE input_buf, JDIMENSION input_row,
		   JSAMPARRAY output_buf, int num_rows)
{
  jcopy_sample_rows(input_buf[0], (int) input_row, output_buf, 0,
		    num_rows, cinfo->output_width);
}


/*
 * Convert grayscale to RGB: just duplicate the graylevel three times.
 * This is provided to support applications that don't want to cope
 * with grayscale as a separate case.
 */

METHODDEF(void)
gray_rgb_convert (j_decompress_ptr cinfo,
		  JSAMPIMAGE input_buf, JDIMENSION input_row,
		  JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      /* We can dispense with GETJSAMPLE() here */
      outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
      outptr += RGB_PIXELSIZE;
    }
  }
}


/*
 * Adobe-style YCCK->CMYK conversion.
 * We convert YCbCr to R=1-C, G=1-M, and B=1-Y using the same
 * conversion as above, while passing K (black) unchanged.
 */

METHODDEF(void)
ycck_cmyk_convert (j_decompress_ptr cinfo,
		   JSAMPIMAGE input_buf, JDIMENSION input_row,
		   JSAMPARRAY output_buf, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2, inptr3;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    inptr3 = input_buf[3][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[0] = range_limit[MAXJSAMPLE - (y + Cr_r_tab[cr])];   /* red */
      outptr[1] = range_limit[MAXJSAMPLE - (y +                   /* green */
				  ((int) RIGHT_SHIFT(Cb_g_tab[cb] + Cr_g_tab[cr],
                         SCALEBITS)))];
      outptr[2] = range_limit[MAXJSAMPLE - (y + Cb_b_tab[cb])];   /* blue */
      /* K passes through unchanged */
      outptr[3] = inptr3[col];	/* don't need GETJSAMPLE here */
      outptr += 4;
    }
  }
}


/*
 * Empty method for start_pass.
 */

METHODDEF(void)
start_pass_dcolor (j_decompress_ptr cinfo)
{
  /* no work needed */
}


/*
 * Module initialization routine for output colorspace conversion.
 */

GLOBAL(void)
jinit_color_deconverter (j_decompress_ptr cinfo)
{
  my_cconvert_ptr cconvert;
  int ci;

  cconvert = (my_cconvert_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_color_deconverter));
  cinfo->cconvert = (struct jpeg_color_deconverter *) cconvert;
  cconvert->pub.start_pass = start_pass_dcolor;

  /* Make sure num_components agrees with jpeg_color_space */
  switch (cinfo->jpeg_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo->num_components != 1)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  case JCS_RGB:
  case JCS_YCbCr:
    if (cinfo->num_components != 3)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  case JCS_CMYK:
  case JCS_YCCK:
    if (cinfo->num_components != 4)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  default:			/* JCS_UNKNOWN can be anything */
    if (cinfo->num_components < 1)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;
  }

  /* Set out_color_components and conversion method based on requested space.
   * Also clear the component_needed flags for any unused components,
   * so that earlier pipeline stages can avoid useless computation.
   */

  switch (cinfo->out_color_space) {
  case JCS_GRAYSCALE:
    cinfo->out_color_components = 1;
    if (cinfo->jpeg_color_space == JCS_GRAYSCALE ||
	cinfo->jpeg_color_space == JCS_YCbCr) {
      cconvert->pub.color_convert = grayscale_convert;
      /* For color->grayscale conversion, only the Y (0) component is needed */
      for (ci = 1; ci < cinfo->num_components; ci++)
	cinfo->comp_info[ci].component_needed = FALSE;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_RGB:
    cinfo->out_color_components = RGB_PIXELSIZE;
    if (cinfo->jpeg_color_space == JCS_YCbCr) {
      cconvert->pub.color_convert = ycc_rgb_convert;
      build_ycc_rgb_table(cinfo);
    } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
      cconvert->pub.color_convert = gray_rgb_convert;
    } else if (cinfo->jpeg_color_space == JCS_RGB && RGB_PIXELSIZE == 3) {
      cconvert->pub.color_convert = null_convert;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_CMYK:
    cinfo->out_color_components = 4;
    if (cinfo->jpeg_color_space == JCS_YCCK) {
      cconvert->pub.color_convert = ycck_cmyk_convert;
      build_ycc_rgb_table(cinfo);
    } else if (cinfo->jpeg_color_space == JCS_CMYK) {
      cconvert->pub.color_convert = null_convert;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  default:
    /* Permit null conversion to same output space */
    if (cinfo->out_color_space == cinfo->jpeg_color_space) {
      cinfo->out_color_components = cinfo->num_components;
      cconvert->pub.color_convert = null_convert;
    } else			/* unsupported non-null conversion */
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;
  }

  if (cinfo->quantize_colors)
    cinfo->output_components = 1; /* single colormapped output component */
  else
    cinfo->output_components = cinfo->out_color_components;
}
