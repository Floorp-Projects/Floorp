/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pangolite
 * dvng-x.c:
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code. The Initial Developer of the Original Code is Sun Microsystems, Inc.  Portions created by SUN are Copyright (C) 2002 SUN Microsystems, Inc. All Rights Reserved.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <glib.h>
#include <string.h>

#include "pango-engine.h"
#include "pango-coverage.h"

#define G_N_ELEMENTS(arr) (sizeof (arr) / sizeof ((arr)[0]))
#define MAX_CLUSTER_CHRS 256
#define MAX_GLYPHS       256
#define GLYPH_COMBINING  256

/*************************************************************************
 *         CHARACTER TYPE CONSTANTS - What they represent                *
 *         ----------------------------------------------                *
 *                                                                       *
 * _NP : Vowel-Modifier Visarg(U+0903) (Displayed to the right).         *
 * _UP : Vowel-Modifer Chandrabindu(U+0901) and Anuswar (U+0902).        *
 *       Displayed above the Consonant/Vowel                             *
 * _IV : Independant Vowels                                              *
 * _CN : All consonants except _CK and _RC below                         *
 * _CK : Consonants that can be followed by a Nukta(U+093C). Characters  *
 *       (U+0958-U+095F). No clue why Unicode imposed it on Devanagari.  *
 * _RC : Consonant Ra - Needs to handle 'Reph'                           *
 * _NM : Handle special case of Matras when followed by a Chandrabindu/  *
 *       Anuswar.                                                        *
 * _IM : Choti I Matra (U+093F). Needs to handle re-ordering.            *
 * _HL : Halant (U+094D)                                                 *
 * _NK : Nukta (U+093C)                                                  *
 * _VD : For Vedic extension characters (U+0951 - U+0954)                *
 * _HD : Hindu Numerals written in Devanagari                            *
 * _MS : For Matras such U+0941, U+0942 which need to be attached to the *
 *       holding consonant at the Stem.                                  *
 * _RM : A cluster of Ra+Halant at the begining would be represented by  *
 *       a Reph (placed in sun.unicode.india-0 at 0xF812). Reph is       *
 *       typically attached on top of the last consonant.                *
 * _II_M, _EY_M, _AI_M, _OW1_M, _OW2_M, _AYE_M, _EE_M, _AWE_M, _O_M,     * 
 *     : Separate glyphs are provided which combine matra with a reph.   *
 *       Matras which need to use this are represented by the above.     *
 *************************************************************************/
 
/*************************************************************************
 *                          CLUSTERING LOGIC                             *
 *                          ----------------                             *
 *                                                                       *
 * Notations used to describe Devanagari Script Generic Info:            *
 * D : Vowel-Modifiers (U+0901 - U+0903)                                 *
 * V : Vowels (U+0905 - U+0913) & (U+0960, U+0961)                       *
 * C : Consonants (U+0915 - U+0939)                                      *
 * M : Matras (U+093E - U+094C) & (U+0962, U+0963)                       *
 * H : Halant (U+094D)                                                   *
 * N : Nukta (U+093C)                                                    *
 *                                                                       *
 * RULES:-                                                               *
 * -------                                                               *
 * Syllable/Cluster types                                                *
 *                                                                       *
 * 1] Vowel Syllable ::- V[D]                                            *
 * 2] Cons - Vowel Syllable ::- [Cons-Syllable] Full- Cons [M] [D]       *
 * 3] Cons-Syllable  ::- [Pure-Cons][Pure-Cons] Pure-Cons                *
 * 4] Pure-Cons  ::- Full-Cons H                                         *
 * 5] Full-Cons  ::- C[N]                                                *
 *                                                                       *
 * Notes:                                                                *
 * 1] Nukta (N) can come after only those consonants with which it can   *
 *    combine, ie U+0915-U+0917, U+091C, U+0921, U+0922, U+092B & U+092F *
 * 2] Worst case Vowel cluster - V D                                     *
 * 3] A worst case Consonant cluster: C N H C N H C N H C N M D          *
 *************************************************************************/

/*
 * Devanagari character classes
 */
#define _NP    1L
#define _UP    (1L<<1)
#define _IV    (1L<<2)
#define _CN    (1L<<3)
#define _CK    (1L<<4)
#define _RC    (1L<<5)
#define _NM    (1L<<6)
#define _IM    (1L<<7)
#define _HL    (1L<<8)
#define _NK    (1L<<9)
#define _VD    (1L<<10)
#define _HD    (1L<<11)
#define _II_M  (1L<<12)
#define _EY_M  (1L<<13)
#define _AI_M  (1L<<14)
#define _OW1_M (1L<<15)
#define _OW2_M (1L<<16)
#define _MS    (1L<<17)
#define _AYE_M (1L<<18)
#define _EE_M  (1L<<19)
#define _AWE_M (1L<<20)
#define _O_M   (1L<<21)
#define _RM    (_II_M|_EY_M|_AI_M|_OW1_M|_OW2_M|_AYE_M|_EE_M|_AWE_M|_O_M)

/* Common Non-defined type */
#define __ND 0

/*
 * Devanagari character types
 */
#define __UP     1
#define __NP     2
#define __IV     3
#define __CN     4
#define __CK     5
#define __RC     6
#define __NM     7
#define __RM     8
#define __IM     9
#define __HL    10
#define __NK    11
#define __VD    12
#define __HD    13

/*
 * Devanagari Glyph Type State
 */
#define St0      0
#define St1      1
#define St2      2
#define St3      3
#define St4      4
#define St5      5
#define St6      6
#define St7      7
#define St8      8
#define St9      9
#define St10    10
#define St11    11
#define St12    12
#define St13    13
#define St14    14
#define St15    15
#define St16    16
#define St17    17
#define St18    18
#define St19    19
#define St20    20

#define _ND     0
#define _NC     1
#define _UC     (1<<1)
#define _BC     (1<<2)
#define _SC     (1<<3)
#define _AV     (1<<4)
#define _BV     (1<<5)
#define _TN     (1<<6)
#define _AD     (1<<7)
#define _BD     (1<<8)
#define _AM     (1<<9)

#define MAP_SIZE     243
#define MAX_STATE     21
#define MAX_DEVA_TYPE 14
#define MAX_CORE_CONS  6

#define SCRIPT_ENGINE_NAME  "DvngScriptEngineX"
#define PANGO_RENDER_TYPE_X "PangoliteRenderX"

#define ucs2dvng(ch) (gunichar2)((gunichar2)(ch) - 0x0900)

typedef guint16 PangoliteXSubfont;
#define PANGO_MOZ_MAKE_GLYPH(index) ((guint32)0 | (index))

/* We handle the following ranges between U+0901 to U+0970 exactly
 */
static PangoliteEngineRange dvng_ranges[] = {
  { 0x0901, 0x0903, "*" },
  { 0x0905, 0x0939, "*" },
  { 0x093c, 0x094d, "*" },
  { 0x0950, 0x0954, "*" },
  { 0x0958, 0x0970, "*" }, /* Hindi Ranges */
};

static PangoliteEngineInfo script_engines[] = {
  {
    SCRIPT_ENGINE_NAME,
    PANGO_ENGINE_TYPE_SHAPE,
    PANGO_RENDER_TYPE_X,
    dvng_ranges, 
    G_N_ELEMENTS(dvng_ranges)
  }
};

/* X window system script engine portion
 */
typedef struct _DvngFontInfo DvngFontInfo;

/* The type of encoding that we will use
 */
typedef enum {
  DVNG_FONT_NONE,
  DVNG_FONT_SUN
} DvngFontType;

/*
 */
typedef struct {
  const gunichar       ISCII[MAX_CORE_CONS];
  const unsigned short ISFOC[MAX_CORE_CONS];
} DvngGlyphEntry;

/*
 */
struct _DvngFontInfo
{
  DvngFontType  type;
  PangoliteXSubfont subfont;
};

typedef long DvngCls;
typedef int  StateType;

/*
 * Devanagari character class table
 */
static const DvngCls DvngChrClsTbl[128] = {
/*            0,       1,      2,        3,      4,       5,    6,   7,
              8,       9,      A,        B,      C,       D,    E,   F, */
/* 0 */     _ND,     _UP,     _UP,     _NP,    _ND,     _IV,   _IV, _IV,
            _IV,     _IV,     _IV,     _IV,    _IV,     _IV,   _IV, _IV,
/* 1 */     _IV,     _IV,     _IV,     _IV,    _IV, _CK|_MS,   _CK, _CK,
            _CN,     _CN,     _CN,     _CN,    _CK,     _CN,   _CN,_CN|_MS,
/* 2 */ _CN|_MS, _CK|_MS, _CK|_MS,     _CN,    _CN,     _CN,   _CN, _CN,
            _CN,     _CN,     _CN, _CK|_MS,    _CN,     _CN,   _CN, _CN,
/* 3 */     _RC,     _CN,     _CN,     _CN,    _CN,     _CN,   _CN, _CN,
            _CN, _CN|_MS,     _ND,     _ND,    _NK,     _VD,   _NM, _IM,
/* 4 */   _II_M,     _NM,     _NM,     _NM,    _NM,  _AYE_M, _EE_M, _EY_M,
          _AI_M,  _AWE_M,    _O_M,  _OW1_M, _OW2_M,     _HL,   _ND, _ND,
/* 5 */     _ND,     _VD,     _VD,     _VD,    _VD,     _ND,   _ND, _ND,
            _CN,     _CN,     _CN,     _CN,    _CN,     _CN,   _CN, _CN,
/* 6 */     _IV,     _IV,     _NM,     _NM,    _ND,     _ND,   _HD, _HD,
            _HD,     _HD,     _HD,     _HD,    _HD,     _HD,   _HD, _HD,
/* 7 */     _ND,     _ND,     _ND,     _ND,    _ND,     _ND,   _ND, _ND,
            _ND,     _ND,     _ND,     _ND,    _ND,     _ND,   _ND, _ND,
};

/*
 * Devanagari character type table
 */
static const gint DvngChrTypeTbl[128] = {
/*         0,    1,    2,    3,    4,    5,    6,    7,
           8,    9,    A,    B,    C,    D,    E,    F, */
/* 0 */ __ND, __UP, __UP, __NP, __ND, __IV, __IV, __IV,
        __IV, __IV, __IV, __IV, __IV, __IV, __IV, __IV,
/* 1 */ __IV, __IV, __IV, __IV, __IV, __CK, __CK, __CK,
        __CN, __CN, __CN, __CN, __CK, __CN, __CN, __CN,
/* 2 */ __CN, __CK, __CK, __CN, __CN, __CN, __CN, __CN,
        __CN, __CN, __CN, __CK, __CN, __CN, __CN, __CN,
/* 3 */ __RC, __CN, __CN, __CN, __CN, __CN, __CN, __CN,
        __CN, __CN, __ND, __ND, __NK, __VD, __NM, __IM,
/* 4 */ __RM, __NM, __NM, __NM, __NM, __RM, __RM, __RM,
        __RM, __RM, __RM, __RM, __RM, __HL, __ND, __ND,
/* 5 */ __ND, __VD, __VD, __VD, __VD, __ND, __ND, __ND,
        __CN, __CN, __CN, __CN, __CN, __CN, __CN, __CN,
/* 6 */ __IV, __IV, __NM, __NM, __ND, __ND, __HD, __HD,
        __HD, __HD, __HD, __HD, __HD, __HD, __HD, __HD,
/* 7 */ __ND, __ND, __ND, __ND, __ND, __ND, __ND, __ND,
        __ND, __ND, __ND, __ND, __ND, __ND, __ND, __ND,
};

/*
 * Devanagari character composible table
 */
static const gint DvngComposeTbl[14][14] = {
  /*        ND, UP, NP, IV, CN, CK, RC, NM, RM, IM, HL, NK, VD, HD,  */
  /* 0  */ { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* ND */
  /* 1  */ { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* UP */
  /* 2  */ { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* NP */
  /* 3  */ { 0,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* IV */
  /* 4  */ { 0,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,}, /* CN */
  /* 5  */ { 0,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,  1,  0,  0,}, /* CK */
  /* 6  */ { 0,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,}, /* RC */
  /* 7  */ { 0,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* NM */
  /* 8  */ { 0,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* RM */
  /* 9  */ { 0,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* IM */
  /* 10 */ { 0,  0,  0,  0,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,}, /* HL */
  /* 11 */ { 0,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,}, /* NK */
  /* 12 */ { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* VD */
  /* 13 */ { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,}, /* HD */
};

StateType DvngStateTbl[MAX_STATE][MAX_DEVA_TYPE] = {

  /* ND,   UP,   NP,   IV,   CN,   CK,   RC,   NM,   RM,
     IM,   HL,   NK,   VD,   HD */
                                           /* State 0 */
 {  St11, St1, St1,  St2,  St4,  St4, St12,  St1,  St1,
    St1,  St1, St1,  St1, St11, },
                                           /* State 1 */
 {  St1,  St1,  St1,  St1,  St1,  St1,  St1,  St1,  St1,
    St1,  St1,  St1,  St1,  St1, },
                                           /* State 2 */
 {  St2,  St3,  St3,  St2,  St2,  St2,  St2,  St2,  St2,
    St2,  St2,  St2,  St2,  St2, },
                                           /* State 3 */
 {  St3,  St3,  St3,  St3,  St3,  St3,  St3,  St3,  St3,
    St3,  St3,  St3,  St3,  St3, },
                                           /* State 4 */
 {  St4,  St8,  St8,  St4,  St4,  St4,  St4,  St6,  St6,
    St9,  St5,  St4,  St4,  St4, },
                                           /* State 5 */
 {  St5,  St5,  St5,  St5,  St4,  St4,  St4,  St5,  St5,
    St5,  St5,  St5,  St5,  St5, },
                                           /* State 6 */
 {  St6,  St7,  St7,  St6,  St6,  St6,  St6,  St6,  St6,
    St6,  St6,  St6,  St6,  St6, },
                                           /* State 7 */
 {  St7,  St7,  St7,  St7,  St7,  St7,  St7,  St7,  St7,
    St7,  St7,  St7,  St7,  St7, },
                                           /* State 8 */
 {  St8,  St8,  St8,  St8,  St8,  St8,  St8,  St8,  St8,
    St8,  St8,  St8,  St8,  St8, },
                                           /* State 9 */
 {  St9, St10, St10,  St9,  St9,  St9,  St9,  St9,  St9,
    St9,  St9,  St9,  St9,  St9, },
                                          /* State 10 */
 { St10, St10, St10, St10, St10, St10, St10, St10, St10,
   St10, St10, St10, St10, St10, },
                                          /* State 11 */
 { St11, St11, St11, St11, St11, St11, St11, St11, St11,
   St11, St11, St11, St11, St11, },
                                          /* State 12 */
 { St12,  St8,  St8, St12, St12, St12, St12,  St6,  St6,
    St9, St13, St12, St12, St12, },
                                          /* State 13 */
 { St13, St13, St13, St13, St14, St14, St14, St13, St13,
   St13, St13, St13, St13, St13, },
                                           /* State 14 */
 { St14, St18, St18, St14, St14, St14, St14, St16, St16,
   St19, St15, St14, St14, St14, },
                                           /* State 15 */
 { St15, St15, St15, St15, St14, St14, St14, St15, St15,
   St15, St15, St15, St15, St15, },
                                           /* State 16 */
 { St16, St17, St17, St16, St16, St16, St16, St16, St16,
   St16, St16, St16, St16, St16, },
                                           /* State 17 */
 { St17, St17, St17, St17, St17, St17, St17, St17, St17,
   St17, St17, St17, St17, St17, },
                                           /* State 18 */
 { St18, St18, St18, St18, St18, St18, St18, St18, St18,
   St18, St18, St18, St18, St18, },
                                           /* State 19 */
 { St19, St20, St20, St19, St19, St19, St19, St19, St19,
   St19, St19, St19, St19, St19, },
                                           /* State 20 */
 { St20, St20, St20, St20, St20, St20, St20, St20, St20,
   St20, St20, St20, St20, St20, },
};

int DvngRuleTbl[128] = {
/*       0,  1,  2,  3,  4,  5,  6,  7,
         8,  9,  A,  B,  C,  D,  E,  F, */

/* 0 */  0,  1,  1,  1,  0,  1,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
/* 1 */  1,  1,  1,  1,  1,  4,  4,  4,
         4,  2,  4,  1,  4,  4,  3,  3,
/* 2 */  3,  3,  3,  2,  4,  4,  4,  4,
         4,  1,  4,  4,  4,  4,  4,  4,
/* 3 */  2,  2,  2,  2,  1,  4,  4,  3,
         5,  4,  0,  0,  2,  1,  1,  1,
/* 4 */  1,  1,  1,  1,  1,  1,  1,  1,
         1,  1,  1,  1,  1,  3,  0,  0,
/* 5 */  1,  1,  1,  1,  1,  0,  0,  0,
         1,  1,  1,  1,  1,  1,  1,  1,
/* 6 */  1,  1,  1,  1,  1,  1,  1,  1,
         1,  1,  1,  1,  1,  1,  1,  1,
/* 7 */  1,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
};

#define IsDvngCharCls(ch, mask) (DvngChrClsTbl[ucs2dvng((ch))] & (mask))

#define IsComposible(ch1, ch2) (DvngComposeTbl[DvngChrTypeTbl[ucs2dvng((ch1))]][DvngChrTypeTbl[ucs2dvng((ch2))]])

#define GetDvngRuleCt(ch) (DvngRuleTbl[ucs2dvng((ch))])

#define IsKern(gid) ((gid >= 0xF830 && gid <= 0xF83E) ? TRUE : FALSE)

#define IsMatraAtStem(gid) (((gid >= 0xF811 && gid <= 0xF813) || \
                             (gid >= 0xF81E && gid <= 0xF82F) || \
                             (gid >= 0x0962 && gid <= 0x0963) || \
                             (gid >= 0x0941 && gid <= 0x0948) || \
                             (gid == 0x093C) || (gid == 0x094D)) ? TRUE : FALSE)

#define SetClusterState(st, ch) DvngStateTbl[(st)][DvngChrTypeTbl[ucs2dvng((ch))]]

#define MAP_SIZE 243

/*
 * Devanagari Glyph Tables
 */
static const DvngGlyphEntry sunGlyphTbl[MAP_SIZE] = {
  /* Vowel Modifiers - Ct 3 */
  { {0x0901,0x0},                       {0x0901,0x0} },
  { {0x0902,0x0},                       {0x0902,0x0} },
  { {0x0903,0x0},                       {0x0903,0x0} },

  /* vowels 13 - Ct 18 */
  { {0x0905,0x0},                       {0x0905,0x0} },
  { {0x0906,0x0},                       {0x0906,0x0} },
  { {0x0907,0x0},                       {0x0907,0x0} },
  { {0x0908,0x0},                       {0x0908,0x0} },
  { {0x0909,0x0},                       {0x0909,0x0} },
  { {0x090a,0x0},                       {0x090a,0xF830,0x0} },
  { {0x090b,0x0},                       {0x090b,0xF831,0x0} },
  { {0x0960,0x0},                       {0x0960,0xF831,0x0} },
  { {0x090c,0x0},                       {0x090c,0xF83D,0x0} },
  { {0x0961,0x0},                       {0x0961,0xF83D,0x0} },
  { {0x090d,0x0},                       {0x090d,0x0} },
  { {0x090e,0x0},                       {0x090e,0x0} },
  { {0x090f,0x0},                       {0x090f,0x0} },
  { {0x0910,0x0},                       {0x0910,0x0} },
  { {0x0911,0x0},                       {0x0911,0x0} },
  { {0x0912,0x0},                       {0x0912,0x0} },
  { {0x0913,0x0},                       {0x0913,0x0} },
  { {0x0914,0x0},                       {0x0914,0x0} },

  /* Vowel signs - Ct 17 */
  { {0x093e,0x0},                       {0x093e,0x0} },
  { {0x093f,0x0},                       {0x093f,0x0} },
  { {0x0940,0x0},                       {0x0940,0x0} },
  { {0x0941,0x0},                       {0x0941,0x0} },
  { {0x0942,0x0},                       {0x0942,0x0} },
  { {0x0943,0x0},                       {0x0943,0x0} },
  { {0x0944,0x0},                       {0x0944,0x0} },
  { {0x0945,0x0},                       {0x0945,0x0} },
  { {0x0946,0x0},                       {0x0946,0x0} },
  { {0x0947,0x0},                       {0x0947,0x0} },
  { {0x0948,0x0},                       {0x0948,0x0} },
  { {0x0949,0x0},                       {0x0949,0x0} },
  { {0x094a,0x0},                       {0x094a,0x0} },
  { {0x094b,0x0},                       {0x094b,0x0} },
  { {0x094c,0x0},                       {0x094c,0x0} },
  { {0x0962,0x0},                       {0x0962,0x0} },
  { {0x0963,0x0},                       {0x0963,0x0} },

  /* Consonants */
  /* ka -> ka + kern space */
  { {0x0915,0x0},                       {0x0915,0xF832,0x0} },
  /* ka nukta  -> ka nukta + kern space */
  { {0x0915,0x093c,0x0},                {0x0958,0xF832,0x0} },
  /* ka + halant  -> half ka */
  { {0x0915,0x094d,0x0},                {0xF7C1,0x0} },
  /* ka nukta + halant-> half ka nukta */
  { {0x0915,0x093c,0x094d,0x0},         {0xF7C2,0x0} },

  { {0x0915,0x094d,0x0915,0x0},         {0xF845,0xF832,0x0} },

  /* ka + halant + ta -> kta + kern space */
  { {0x0915,0x094d,0x0924,0x0},         {0xF7C4,0xF832,0x0} },
  /* ka + halant + ra -> kra + kern space */
  { {0x0915,0x094d,0x0930,0x0},         {0xF7C3,0xF832,0x0} },
  { {0x0915,0x094d,0x0930,0x094d,0x0},  {0xF7C3,0x094d,0xF832,0x0} },
  /* ka + halant + SHa -> kSHa */
  { {0x0915,0x094d,0x0937,0x0},         {0xF7C5,0x093E,0x0} }, 
  /* ka + halant + SHa + halant -> half kSHa */
  { {0x0915,0x094d,0x0937,0x094d,0x0},  {0xF7C5,0x0} },

  /* kha 6 */
  { {0x0916,0x0},                       {0x0916,0x0} },
  { {0x0916,0x094d,0x0},                {0xF7C6,0x0} },
  { {0x0916,0x093c,0x0},                {0x0959,0x0} },
  { {0x0916,0x093c,0x094d,0x0},         {0xF7C7,0x0} },
  { {0x0916,0x094d,0x0930,0x0},         {0xF7C8,0x093E,0x0} },
  { {0x0916,0x094d,0x0930,0x094d,0x0},  {0xF7C8,0x0} },

  /* ga 6 */
  { {0x0917,0x0},                       {0x0917,0x0} },
  { {0x0917,0x094d,0x0},                {0xF7C9,0x0} },
  { {0x0917,0x093c,0x0},                {0x095a,0x0} },
  { {0x0917,0x093c,0x094d,0x0},         {0xF7CA,0x0} },
  { {0x0917,0x094d,0x0930,0x0},         {0xF7CB,0x093E,0x0} },
  { {0x0917,0x094d,0x0930,0x094d,0x0},  {0xF7CB,0x0} },

  /* gha 4 */
  { {0x0918,0x0},                       {0x0918,0x0} },
  { {0x0918,0x094d,0x0},                {0xF7CC,0x0} },
  { {0x0918,0x094d,0x0930,0x0},         {0xF7CD,0x093E,0x0} },
  { {0x0918,0x094d,0x0930,0x094d,0x0},  {0xF7CD,0x0} },

  /* nga 1 */
  { {0x0919,0x0},                       {0x0919,0xF833,0x0} },

  /* { {0x0919,0x094d,0x0},             {0x0919,0x094d,0xF833,0x0} }, */

  /* cha 4 */
  { {0x091a,0x0},                       {0x091a,0x0} },
  /* cha  + halant -> half cha   */
  { {0x091a,0x094d,0x0},                {0xF7CE,0x0} },
  /* cha + halant ra -> chra  */
  { {0x091a,0x094d,0x0930,0x0},         {0xF7CF,0x093E,0x0} },
  /* cha  + halant ra + halant  */
  { {0x091a,0x094d,0x0930,0x094d,0x0},  {0xF7CF,0x0} },

  /* chha 1 */
  { {0x091b,0x0},                       {0x091b,0xF834,0x0} },

  /* chha + halant -> chha+ halant */
  /*  { {0x091b,0x094d,0x0 },           {0x091b,0x094d,0xF834,0x0} }, */

  /* ja 6 */
  /* ja  */
  { {0x091c,0x0},                       {0x091c,0x0} },
  /* ja + halant -> half ja  */
  { {0x091c,0x094d,0x0},                {0xF7D0,0x0} },
  /* ja + nukta -> za */
  { {0x091c,0x093c,0x0},                {0xF7D1,0x093E,0x0} },
  /* ja + nukta + halant -> half za */
  { {0x091c,0x093c,0x094d,0x0 },        {0xF7D1,0x0} },
  /* ja + halant + ra -> jra  */
  { {0x091c,0x094d,0x0930,0x0},         {0xF7D2,0x093E,0x0} },
  /* ja + halant + ra + halant -> */
  { {0x091c,0x094d,0x0930,0x094d,0x0},  {0xF7D2,0x0} },

  /* dna 2 */
  /* ja + halant + jna -> dna  */
  { {0x091c,0x094d,0x091e,0x0},         {0xF7D3,0x093E,0x0} },
  /* ja + halant + jna -> half dna  */
  { {0x091c,0x094d,0x091e,0x094d,0x0},  {0xF7D3,0x0} },

  /* jha 4 */
  /* jha  */
  { {0x091d,0x0},                       {0x091d,0x0} },
  /* jha + halant -> half jha  */
  { {0x091d,0x094d,0x0},                {0xF7D4,0x0} },
  /* jha + halant -> jhra */
  { {0x091d,0x094d,0x0930,0x0},         {0xF7D5,0x093E,0x0} },
  /*  jha + halant -> half jhra */
  { {0x091d,0x094d,0x0930,0x094d,0x0},  {0xF7D5,0x0} },

  /* nya 2 */
  /* nya */
  { {0x091e,0x0},                       {0x091e,0x0} },
  /* nya + halant -> half nya */
  { {0x091e,0x094d,0x0},                {0xF7D6,0x0} },

  { {0x091e,0x094d,0x091c,0x0},         {0xF846,0x0} },

  /* Ta 3 */
  /* Ta -> Ta + kern space */
  { {0x091f,0x0},                       {0x091F,0xF835,0x0} },

  { {0x091f,0x094d,0x0},                {0x091f,0x094d,0xF835,0x0} },

  /* Ta + halant + Ta -> TaTa + kern space */
  { {0x091f,0x094d,0x091f,0x0},         {0xF7D7,0xF835,0x0} },
  /* Ta + halant + Tha -> TaTha + kern space */
  { {0x091f,0x094d,0x0920,0x0},         {0xF7D8,0xF835,0x0} },

  /* Tha 2 */
  /* Tha -> Tha + kern space */
  { {0x0920,0x0},                       {0x0920,0xF836,0x0} },

  { {0x0920,0x094d,0x0},                {0x0920,0x094d,0xF836,0x0} },

  /* Tha + halant + Tha -> + kern space */
  { {0x0920,0x094d,0x0920,0x0},         {0xF7D9,0xF836,0x0} },

  /* Da 1 */
  /* Da  -> Da  + kern space */
  { {0x0921,0x0},                       {0x0921,0xF837,0x0} },

  { {0x0921,0x094d,0x0},                {0x0921,0x094d,0xF837,0x0} },
 
  /* Da nukta 1 */
  /* Da + nukta -> + kern space */
  { {0x0921,0x093c,0x0},                {0x095c,0xF837,0x0} },

  /* Da + nukta+ halant */
  { {0x0921,0x093c,0x094d,0x0},         {0x095c,0x094d,0xF837,0x0} },

  { {0x0921,0x094d,0x0917,0x0},         {0xF847,0xF837,0x0} },

  /* Da halant Da 1 */
  /* Da + halant + Da - > + kern space */
  { {0x0921,0x094d,0x0921,0x0},         {0xF7DA,0xF837,0x0} },

  /* Da halant Dha 1 */
  /* Da + halant + Dha -> + kern space */
  { {0x0921,0x094d,0x0922,0x0},         {0xF7DB,0xF837,0x0} },

  /* Dha 1 */
  /* Dha  + kern space */
  { {0x0922,0x0},                       {0x0922,0xF838,0x0} },

  /* Dha nukta 1 */
  /* Dha + nukta -> + kern space */
  { {0x0922,0x093c,0x0},                {0x095d,0xF838,0x0} },
  { {0x0922,0x093c,0x094d,0x0},         {0x095d,0x094d,0xF838,0x0} },

  { {0x0922,0x094d,0x0},                {0x0922,0x094d,0xF838,0x0} },

  /* Nna 2 */
  /* Nna */
  { {0x0923,0x0},                       {0x0923,0x0} },
  /* Nna + halant -> half Nna */
  { {0x0923,0x094d,0x0},                {0xF7DC,0x0} },

  /* ta 6 */
  /* ta */
  { {0x0924,0x0},                       {0x0924,0x0} },
  /* ta + halant -> half ta */
  { {0x0924,0x094d,0x0},                {0xF7DD,0x0} },
  /* ta + halant + ra -> half tra */
  { {0x0924,0x094d,0x0930,0x0},         {0xF7DE,0x093E,0x0} },
  /* ta + halant + ra + halant -> half tra  */
  { {0x0924,0x094d,0x0930,0x094d,0x0},  {0xF7DE,0x0} },
  /* ta + halant + ta -> */
  { {0x0924,0x094d,0x0924,0x0},         {0xF7DF,0x093E,0x0} },
  /* ta + halant + ta + halant -> */
  { {0x0924,0x094d,0x0924,0x094d,0x0},  {0xF7DF,0x0} },

  /* tha 4 */
  /* tha */
  { {0x0925,0x0},                       {0x0925,0x0} },
  /* tha + halant -> half tha */
  { {0x0925,0x094d,0x0},                {0xF7E0,0x0} },
  /* tha + halant + ra ->  */
  { {0x0925,0x094d,0x0930,0x0},         {0xF7E1,0x093E,0x0} },
  /* tha + halant + ra + halant -> */
  { {0x0925,0x094d,0x0930,0x094d,0x0},  {0xF7E1,0x0} },

  /* da 1 */
  /* da -> da + kern space */
  { {0x0926,0x0},                       {0x0926,0xF839,0x0} },
  /* da + halant -> half da + kern space */
  { {0x0926,0x094d,0x0},                {0x0926,0x094d,0xF839,0x0} },

  /* da ri 1 */
  /* da + ru -> da + ru + kern space */
  { {0x0926,0x0943,0x0},                {0xF7E2,0xF839,0x0} },

  /* da halant ra 1 */
  /* da + halant + ra -> dra + kern space  */
  { {0x0926,0x094d,0x0930,0x0},         {0xF7E3,0xF839,0x0} },
  { {0x0926,0x094d,0x0930,0x094d,0x0},  {0xF7E3,0x094d,0xF839,0x0} },

  { {0x0926,0x094d,0x0918,0x0},         {0xF848,0xF839,0x0} },

  /* da halant da 1 */
  /* da + halant + da  -> + kern space */
  { {0x0926,0x094d,0x0926,0x0},         {0xF7E4,0xF839,0x0} },

  /* da halant dha 1 */
  /* da + halant + dha -> + kern space  */
  { {0x0926,0x094d,0x0927,0x0},         {0xF7E5,0xF839,0x0} },

  { {0x0926,0x094d,0x092c,0x0},         {0xF849,0xF839,0x0} },

  { {0x0926,0x094d,0x092d,0x0},         {0xF844,0xF839,0x0} },

  /* da halant ma 1 */
  /* da + halant + ma -> + kern space */
  { {0x0926,0x094d,0x092e,0x0},         {0xF7E6,0xF839,0x0} },

  /* da halant ya 1 */
  /* da + halant + ya -> + kern space */
  { {0x0926,0x094d,0x092f,0x0},         {0xF7E7,0xF839,0x0} },

  /* da halant va 1 */
  /* da + halant + va -> + kern space  */
  { {0x0926,0x094d,0x0935,0x0},         {0xF7E8,0xF839,0x0} },

  /* dha 4 */
  /* Dha  */
  { {0x0927,0x0},                       {0x0927,0x0} },
  /* Dha + halant - > half Dha */
  { {0x0927,0x094d,0x0},                {0xF7E9,0x0} },
  /* Dha + halant + ra -> half Dhra */
  { {0x0927,0x094d,0x0930,0x0},         {0xF7EA,0x093E,0x0} },
  /* Dha + halant + ra + halant ->  */
  { {0x0927,0x094d,0x0930,0x094d,0x0},  {0xF7EA,0x0} },

  /* na 6 */
  /* na */
  { {0x0928,0x0},                       {0x0928,0x0} },
  /* na + halant  ->  half na */
  { {0x0928,0x094d,0x0},                {0xF7EB,0x0} },
  /* na + halant + ra  -> */
  { {0x0928,0x094d,0x0930,0x0},         {0xF7EC,0x093E,0x0} },
  /* na + halant + ra + halant -> */
  { {0x0928,0x094d,0x0930,0x094d,0x0},  {0xF7EC,0x0} },
  /* na + halant + na -> */
  { {0x0928,0x094d,0x0928,0x0},         {0xF7ED,0x093E,0x0} },
  /* na + halant + na + halant -> */
  { {0x0928,0x094d,0x0928,0x094d,0x0},  {0xF7ED,0x0} },

  { {0x0929,0x0},                       {0x0929,0x0} },

  /* pa 4 */
  /* pa */
  { {0x092a,0x0},                       {0x092a,0x0} },
  /* pa + halant -> half pa */
  { {0x092a,0x094d,0x0},                {0xF7EE,0x0} },
  /* pa + halant +ra -> pra */
  { {0x092a,0x094d,0x0930,0x0},         {0xF7EF,0x093E,0x0} },
  /* pa + halant + ra + halant -> half pra */
  { {0x092a,0x094d,0x0930,0x094d,0x0},  {0xF7EF,0x0} },

  /* pha 5 */
  /* pha -> pha + kern space */
  { {0x092b,0x0 },                      {0x092b,0xF832,0x0} },
  /* pha + halant -> half pha */
  { {0x092b,0x094d,0x0},                {0xF7F0,0x0} },
  /* pha nukta -> pha nukta + kern space */
  { {0x092b,0x093c,0x0},                {0x095e,0xF832,0x0} },
  /* pha nukta + halant-> half pha nukta */
  { {0x092b,0x093c,0x094d,0x0},         {0xF7F1,0x0} },
  /* pha + halant + ra -> fra + kern space */
  { {0x092b,0x094d,0x0930,0x0},         {0xF7F5,0xF832,0x0} },
  /* pha + halant + ra -> fra + kern space */
  { {0x092b,0x094d,0x0930,0x094d,0x0},  {0xF7F5,0xF832,0x094d,0x0} },

  /* ba 4 */
  /* ba */
  { {0x092c,0x0},                       {0x092c,0x0} },
  /* ba + halant -> half ba */
  { {0x092c,0x094d,0x0},                {0xF7F6,0x0} },
  /* ba + halant + ra -> */
  { {0x092c,0x094d,0x0930,0x0},         {0xF7F7,0x093E,0x0} },
  /* ba + halant ra + halant -> */
  { {0x092c,0x094d,0x0930,0x094d,0x0},  {0xF7F7,0x0} },

  /* bha 4 */
  /* bha  */
  { {0x092d,0x0},                       {0x092d,0x0} },
  /* bha + halant -> half halant  */
  { {0x092d,0x094d,0x0},                {0xF7F8,0x0} },
  /* bha + halant + ra ->  */
  { {0x092d,0x094d,0x0930,0x0},         {0xF7F9,0x093E,0x0} },
  /* bha + halant + ra + halant ->  */
  { {0x092d,0x094d,0x0930,0x094d,0x0},  {0xF7F9,0x0} },
  /* ma 4 */
  /* ma  */
  { {0x092e,0x0},                       {0x092e,0x0} },
  /* ma + halant -> half ma */
  { {0x092e,0x094d,0x0},                {0xF7FA,0x0} },
  /* ma + halant + ra -> */
  { {0x092e,0x094d,0x0930,0x0},         {0xF7FB,0x093E,0x0} },
  /* ma + halant + ra + halant ->  */
  { {0x092e,0x094d,0x0930,0x094d,0x0},  {0xF7FB,0x0} },

  /* ya 4 */
  /* ya */
  { {0x092f,0x0},                       {0x092f,0x0} },
  /* ya + halant -> half ya */
  { {0x092f,0x094d,0x0},                {0xF7FC,0x0} },
  /* ya + halant + ra -> */
  { {0x092f,0x094d,0x0930,0x0},         {0xF7FD,0x093E,0x0} },
  /* ya + halant + ra + halant -> */
  { {0x092f,0x094d,0x0930,0x094d,0x0},  {0xF7FD,0x0} },

  /* ra 3 */
  /* ra  */
  { {0x0930,0x0 },                      {0x0930,0xF83A,0x0} },
  /*  { {0x0930,0x094d,0x0},                 {0xF812,0x0} }, */
  /* ra + u -> Ru + kern space */
  { {0x0930,0x0941,0x0},                {0xF800,0xF83B,0x0} },
  /* ra + U -> RU + kern space */
  { {0x0930,0x0942,0x0},                {0xF801,0xF83C,0x0} },

  { {0x0931,0x0},                       {0x0931,0x0} },
  { {0x0931,0x094d,0x0},                {0xF7FF,0x0} },

  /* la 2 */
  /* la  */
  { {0x0932,0x0},                       {0x0932,0x0} },
  /* la + halant -> */
  { {0x0932,0x094d,0x0},                {0xF802,0x0} },
  /* La 2 */
  /* La */
  { {0x0933,0x0},                       {0x0933,0x0} },
  /* La + halant -> half La */
  { {0x0933,0x094d,0x0},                {0xF803,0x0} },

  { {0x0934,0x0} ,                      {0x0934,0x0} },
  /* va 4 */
  /* va */
  { {0x0935,0x0},                       {0x0935,0x0} },
  /* va + halant -> half va */
  { {0x0935,0x094d,0x0},                {0xF804,0x0} },
  /* va + halant + ra ->  */
  { {0x0935,0x094d,0x0930,0x0},         {0xF805,0x093E,0x0} },
  /* va + halant + ra + halant ->   */
  { {0x0935,0x094d,0x0930,0x094d,0x0},  {0xF805,0x0} },

  /* sha 6 */
  /* sha */
  { {0x0936,0x0},                       {0x0936,0x0} },
  /* sha + halant -> half sha */
  { {0x0936,0x094d,0x0},                {0xF806,0x0} },

  { {0x0936,0x094d,0x091a,0x0},         {0xF83F,0x0} },
  { {0x0936,0x094d,0x0928,0x0},         {0xF840,0x0} },

  /* sha + halant + va -> shwa */
  { {0x0936,0x094d,0x0935,0x0},         {0xF807,0x093E,0x0} },
  /* sha + halant + va + halant -> half shwa */
  { {0x0936,0x094d,0x0935,0x094d,0x0},  {0xF807,0x0} },
  /* sha + halant + ra -> shra */
  { {0x0936,0x094d,0x0930,0x0},         {0xF808,0x093E,0x0} },
  /* sha + halant + ra + halant -> half shra */
  { {0x0936,0x094d,0x0930,0x094d,0x0},  {0xF808,0x0} },
  /* SHa 2 */
  /* SHa */
  { {0x0937,0x0},                       {0x0937,0x0} },
  /* SHa + halant -> half SHa  */
  { {0x0937,0x094d,0x0},                {0xF809,0x0} },

  { {0x0937,0x094d,0x091f,0x0},         {0xF841,0xF835,0x0} },
  { {0x0937,0x094d,0x0920,0x0},         {0xF842,0xF836,0x0} },

  /* sa 4 */
  /* sa */
  { {0x0938,0x0},                       {0x0938,0x0} },
  /* sa + halant -> half sa */
  { {0x0938,0x094d,0x0},                {0xF80A,0x0} },
  /* sa + halant + ra ->  */
  { {0x0938,0x094d,0x0930,0x0},         {0xF80B,0x093E,0x0} },
  /* sa + halan + ra + halant -> */
  { {0x0938,0x094d,0x0930,0x094d,0x0},  {0xF80B,0x0} },

  { {0x0938,0x094d,0x0924,0x094d,0x0930,0x0}, {0xF843,0x0} },

  /* ha 2 */
  /* ha + kern space  */
  { {0x0939,0x0},                       {0x0939,0xF83E,0x0} },
  /* ha + halant  -> half ha */
  { {0x0939,0x094d,0x0},                {0xF80C,0xF83E,0x0} },
  /* ha + Rii + matra -> */
  { {0x0939,0x0943,0x0},                {0xF80D,0xF83E,0x0} },
  /* ha + halant + ra -> */
  { {0x0939,0x094d,0x0930,0x0},         {0xF80E,0xF83E,0x0} },
  /* ha + halant + ra + halant -> */
  { {0x0939,0x094d,0x0930,0x094d,0x0},  {0xF80E,0x094d,0xF83E,0x0} },

  { {0x0939,0x094d,0x0923,0x0},         {0xF84D,0xF83E,0x0} },
  { {0x0939,0x094d,0x0928,0x0},         {0xF84C,0xF83E,0x0} },

  /* ha + halant + ma -> */
  { {0x0939,0x094d,0x092e,0x0},         {0xF80F,0xF83E,0x0} },
  /* ha + halant + ya -> */
  { {0x0939,0x094d,0x092f,0x0},         {0xF810,0xF83E,0x0} },

  { {0x0939,0x094d,0x0932,0x0},         {0xF84A,0xF83E,0x0} },
  { {0x0939,0x094d,0x0935,0x0},         {0xF84B,0xF83E,0x0} },

  { {0x0958,0x0},                       {0x0958,0xF832,0x0} },
  { {0x0959,0x0},                       {0x0959,0x0} },
  { {0x095a,0x0},                       {0x095a,0x0} },
  { {0x095b,0x0},                       {0x095b,0x0} },
  { {0x095c,0x0},                       {0x095c,0xF837,0x0} },
  { {0x095d,0x0},                       {0x095d,0xF838,0x0} },
  { {0x095e,0x0},                       {0x095e,0xF832,0x0} },
  { {0x095f,0x0},                       {0x095f,0x0} },

  /*"\xd8\x", "", */

  /* misc 5 */

  /* nukta */
  { {0x093c,0x0},                       {0x093c,0x0} },
  /* nukta + u matra */
  { {0x093c,0x0941,0x0},                {0xF81E,0x0} },
  /* nukta + Uu matra -> */
  { {0x093c,0x0942,0x0},                {0xF821,0x0} },

  /* halant */
  { {0x094d,0x0},                       {0x094d,0x0} },
  /* halant + ya -> */
  { {0x094d,0x092f,0x0},                {0xF7FE,0x0} },
  /* halant + ra  */
  { {0x094d,0x0930,0x0},                {0xF811,0x0} },
  /* halant + ra + halant */
  { {0x094d,0x0930,0x094d,0x0},         {0x094d,0x0930,0x0930,0x0} },
  /* halant + ra + u matra -> */
  { {0x094d,0x0930,0x0941,0x0},         {0xF81F,0x0} },
  /* halant + ra + Uu matra ->  */
  { {0x094d,0x0930,0x0942,0x0},         {0xF822,0x0} },

  /* Vedic Characters */
  { {0x093d,0x0},                       {0x093d,0x0} },
  { {0x0950,0x0},                       {0x0950,0x0} },
  { {0x0951,0x0},                       {0x0951,0x0} },
  { {0x0952,0x0},                       {0x0952,0x0} },
  { {0x0953,0x0},                       {0x0953,0x0} },
  { {0x0954,0x0},                       {0x0954,0x0} },
  { {0x0964,0x0},                       {0x0964,0x0} },
  { {0x0965,0x0},                       {0x0965,0x0} },
  { {0x0970,0x0},                       {0x0970,0x0} },

  /* Dig09its */
  { {0x0966,0x0},                       {0x0966,0x0} },
  { {0x0967,0x0},                       {0x0967,0x0} },
  { {0x0968,0x0},                       {0x0968,0x0} },
  { {0x0969,0x0},                       {0x0969,0x0} },
  { {0x096a,0x0},                       {0x096a,0x0} },
  { {0x096b,0x0},                       {0x096b,0x0} },
  { {0x096c,0x0},                       {0x096c,0x0} },
  { {0x096d,0x0},                       {0x096d,0x0} },
  { {0x096e,0x0},                       {0x096e,0x0} },
  { {0x096f,0x0},                       {0x096f,0x0} }
};

/* Returns a structure with information we will use to render given the
 * #PangoliteFont. This is computed once per font and cached for retrieval.
 */
static DvngFontInfo *
get_font_info(const char *fontCharset)
{
  static const char *charsets[] = {
    "sun.unicode.india-0",
  };

  static const int charset_types[] = {
    DVNG_FONT_SUN
  };
  
  DvngFontInfo *font_info = g_new(DvngFontInfo, 1);
  guint        i; 

  font_info->type = DVNG_FONT_NONE;
  for (i = 0; i < G_N_ELEMENTS(charsets); i++) {
    if (strcmp(fontCharset, charsets[i]) == 0) {    
      font_info->type = (DvngFontType)charset_types[i];
      font_info->subfont = (PangoliteXSubfont)i;
      break;
    }
  }
  
  return font_info;
}

static void
add_glyph(PangoliteGlyphString *glyphs,
          gint                 clusterStart,
          PangoliteGlyph       glyph,
          gint                 combining)
{
  gint index = glyphs->num_glyphs;

  if ((clusterStart == 0) && (index != 0))
    clusterStart++;

  pangolite_glyph_string_set_size (glyphs, index + 1);  
  glyphs->glyphs[index].glyph = glyph;
  glyphs->glyphs[index].is_cluster_start = combining;
  glyphs->log_clusters[index] = clusterStart;
}

static void
GetBaseConsGlyphs(gunichar2  *cluster,
                  gint       numCoreCons,
                  PangoliteGlyph *glyphList,
                  gint       *nGlyphs)
{
  int i, j, delta, nMin, nMaxRuleCt, ruleIdx;
  gboolean  StillMatching;
  gunichar2 temp_out;
  gint      tmpCt = *nGlyphs;

  i = 0;
  while (i < numCoreCons) {
    
    nMaxRuleCt = GetDvngRuleCt(cluster[i]);    
    while (nMaxRuleCt) {
      nMin = MIN(nMaxRuleCt, numCoreCons);      
      ruleIdx = 0;

NotFound:
      j = delta = 0;
      StillMatching = FALSE;
      while (((delta < nMin) || sunGlyphTbl[ruleIdx].ISCII[j]) &&
             (ruleIdx < MAP_SIZE) ) {
        StillMatching = TRUE;
        if ((delta < nMin) && (j < MAX_CORE_CONS) &&
            (cluster[i + delta] != sunGlyphTbl[ruleIdx].ISCII[j])) {
          ruleIdx++;
          goto NotFound;
        }
        delta++;
        j++;
      }
      
      if (StillMatching) /* Found */
        break;
      else
        nMaxRuleCt--;
    }
    
    i += nMin;
    
    /* Can't find entry in the table */
    if ((StillMatching == FALSE) || (ruleIdx >= MAP_SIZE)) {
      for (j = 0; j < numCoreCons; j++)
        glyphList[tmpCt++] = PANGO_MOZ_MAKE_GLYPH(cluster[j]);
    }
    else if (((tmpCt > 0) && IsKern(glyphList[tmpCt - 1])) &&
             IsMatraAtStem(sunGlyphTbl[ruleIdx].ISFOC[0])) {
      temp_out = glyphList[tmpCt - 1];
      
      for (j=0; sunGlyphTbl[ruleIdx].ISFOC[j]; j++)
        glyphList[tmpCt - 1] = PANGO_MOZ_MAKE_GLYPH(sunGlyphTbl[ruleIdx].ISFOC[j]);
        
      glyphList[tmpCt++] = PANGO_MOZ_MAKE_GLYPH(temp_out);
    }
    else {
      for (j=0; sunGlyphTbl[ruleIdx].ISFOC[j]; j++)
        glyphList[tmpCt++] = PANGO_MOZ_MAKE_GLYPH(sunGlyphTbl[ruleIdx].ISFOC[j]);
    }
  }
  *nGlyphs = tmpCt;
}

static gint
get_adjusted_glyphs_list(DvngFontInfo *fontInfo,
                         gunichar2    *cluster,
                         gint         nChars,
                         PangoliteGlyph   *gLst,
                         StateType    *DvngClusterState)
{
  int i, k, len;
  gint      nGlyphs = 0;
  gunichar2 dummy;
  
  switch (*DvngClusterState) {
  case St1:
    if (IsDvngCharCls(cluster[0], _IM)) {
      GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF7C0);
    }
    else {
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF7C0);
      GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);
    }
    break;

    case St2:
    case St3:
    case St4:
    case St5:
    case St6:
      GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);
      break;
      
    case St7:
      if (IsDvngCharCls(cluster[nChars - 1], _UP)) {

        if (IsDvngCharCls(cluster[nChars - 2], _RM))
          GetBaseConsGlyphs(cluster, nChars - 2, gLst, &nGlyphs);
        else
          GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);

        if (IsDvngCharCls(cluster[nChars - 2], _RM)) {

          if (IsDvngCharCls(cluster[nChars - 2], _II_M))
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF81B);

          else if (IsDvngCharCls(cluster[nChars - 2], _AYE_M)) {
            dummy = gLst[nGlyphs - 1];
            gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(0xF82D);
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
          }

          else if (IsDvngCharCls(cluster[nChars - 2], _EE_M))
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF824);
          
          else if (IsDvngCharCls(cluster[nChars - 2], _EY_M))
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF827);

          else if (IsDvngCharCls(cluster[nChars - 2], _AI_M))
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82A);

          else if (IsDvngCharCls(cluster[nChars - 2], _AWE_M)) {
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82D);
          }

          else if (IsDvngCharCls(cluster[nChars - 2], _O_M)) {
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF824);
          }

          else if (IsDvngCharCls(cluster[nChars - 2], _OW1_M)) {
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF827);
          }

          else if (IsDvngCharCls(cluster[nChars - 2], _OW2_M)) {
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
            gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82A);
          }
        }
      }
      else {
        GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);
      }
      break;

    case St8:
      GetBaseConsGlyphs(cluster, nChars - 1, gLst, &nGlyphs);
      if (IsKern(gLst[nGlyphs - 1])) {
        dummy = gLst[nGlyphs - 1];
        gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(cluster[nChars - 1]);
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
      }
      else
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(cluster[nChars - 1]);
      break;
      
    case St9:
      if (IsDvngCharCls(cluster[0], _MS))
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093F);
      else
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF817);        

      GetBaseConsGlyphs(cluster, nChars - 1, gLst, &nGlyphs);
      break;

  case St10:
    if (IsDvngCharCls(cluster[0], _MS))
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF814);
    else
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF818);

    GetBaseConsGlyphs(cluster, nChars - 2, gLst, &nGlyphs);
    break;
    
  case St11:
    GetBaseConsGlyphs(cluster, nChars, gLst, &nGlyphs);
    break;
    
  case St12:
    gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x0930);
    gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF83A);
    break;
    
  case St13:
    gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x0930);
    gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x094D);
    gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF83A);
    break;
    
  case St14:    
    GetBaseConsGlyphs(cluster+2, nChars - 2, gLst, &nGlyphs);
    if (IsKern(gLst[nGlyphs - 1])) {
      dummy = gLst[nGlyphs - 1];
      gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(0xF812);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
    }
    else
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF812);
    break;
    
  case St15:    
    GetBaseConsGlyphs(cluster+2, nChars - 3, gLst, &nGlyphs);
    if (IsKern(gLst[nGlyphs - 1])) {      
      dummy = gLst[nGlyphs - 2];
      gLst[nGlyphs - 2] = PANGO_MOZ_MAKE_GLYPH(0xF812);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x094D);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
    }
    else {
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF812);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x094D);
    }
    break;
    
  case St16:
    if (IsDvngCharCls(cluster[nChars - 1], _RM))
      GetBaseConsGlyphs(cluster+2, nChars - 3, gLst, &nGlyphs);
    else
      GetBaseConsGlyphs(cluster+2, nChars - 2, gLst, &nGlyphs);

    if (IsDvngCharCls(cluster[nChars - 1], ~(_RM))){

      if (IsKern(gLst[nGlyphs - 1])) {
        dummy = gLst[nGlyphs - 1];
        gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(0xF812);
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
      }
      else
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF812);
    }
    else {

      if (IsDvngCharCls(cluster[nChars - 1], _II_M))
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF81C);

      else if (IsDvngCharCls(cluster[nChars - 1], _EY_M))
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF828);

      else if (IsDvngCharCls(cluster[nChars -1], _AI_M))
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82B);

      else if (IsDvngCharCls(cluster[nChars - 1], _OW1_M)) {
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF828);
      }

      else if (IsDvngCharCls(cluster[nChars - 1], _OW2_M)) {
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
        gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82B);
      }
    }
    break;
    
  case St17:
    if (IsDvngCharCls(cluster[nChars - 1], _UP)) {

      if (IsDvngCharCls(cluster[nChars - 2], _RM))
        GetBaseConsGlyphs(cluster+2, nChars - 4, gLst, &nGlyphs);
      else
        GetBaseConsGlyphs(cluster+2, nChars - 3, gLst, &nGlyphs);

      if (IsDvngCharCls(cluster[nChars - 2], ~(_RM))) {

        if (IsKern(gLst[nGlyphs - 1])) {
          dummy = gLst[nGlyphs - 1];
          gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(0xF813);
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
        }
        else
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF813);
      }
      else {
        if (IsDvngCharCls(cluster[nChars - 2], _II_M))
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF81D);

        else if (IsDvngCharCls(cluster[nChars - 2], _EY_M))
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF829);

        else if (IsDvngCharCls(cluster[nChars - 2], _AI_M))
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82C);

        else if (IsDvngCharCls(cluster[nChars - 2], _OW1_M)) {
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF829);
        }
        else if (IsDvngCharCls(cluster[nChars - 2], _OW2_M)) {
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0x093E);
          gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF82C);
        }
      } /* ? else GetBaseConsGlyphs(); */
    break;
    
  case St18:
    GetBaseConsGlyphs(cluster-2, nChars-3, gLst, &nGlyphs);
    if (IsKern(gLst[nGlyphs - 1])) {
      dummy = gLst[nGlyphs - 1];
      gLst[nGlyphs - 1] = PANGO_MOZ_MAKE_GLYPH(0xF813);
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(dummy);
    }
    else
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF813);
    break;
    
  case St19:
    if (IsDvngCharCls(cluster[0], _MS))
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF815);
    else
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF819);

    GetBaseConsGlyphs(cluster+2, nChars-3, gLst, &nGlyphs);
    break;
    
  case St20:
    if (IsDvngCharCls(cluster[0], _MS))
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF816);
    else
      gLst[nGlyphs++] = PANGO_MOZ_MAKE_GLYPH(0xF81A);

    GetBaseConsGlyphs(cluster+2, nChars - 4, gLst, &nGlyphs);
    break;
    }
  }

  return nGlyphs;
}

static gint
get_glyphs_list(DvngFontInfo *fontInfo,
                gunichar2    *cluster,
                gint          numChars,
                PangoliteGlyph   *glyphLst,
                StateType    *clustState)
{
  PangoliteGlyph glyph;
  gint       i;

  switch (fontInfo->type) {
  case DVNG_FONT_NONE:
    for (i = 0; i < numChars; i++)
      glyphLst[i] = 0; /*pangolite_x_get_unknown_glyph(fontInfo->font);*/
    return numChars;
    
  case DVNG_FONT_SUN:
    return get_adjusted_glyphs_list(fontInfo, cluster, numChars, glyphLst, clustState);
  }
  
  return 0; /* Quiet GCC */
}

static void
add_cluster(DvngFontInfo     *fontInfo,
            PangoliteGlyphString *glyphs,
            gint              clusterBeg,
            gunichar2        *cluster,
            gint              numChars,
            StateType        *clustState)
{
  PangoliteGlyph glyphsList[MAX_GLYPHS];
  gint           i, numGlyphs, ClusterStart=0;
  
  numGlyphs = get_glyphs_list(fontInfo, cluster, numChars, glyphsList, clustState);
  for (i = 0; i < numGlyphs; i++) {

    ClusterStart = (gint)GLYPH_COMBINING;
    if (i == 0)
      ClusterStart = numChars;
    add_glyph(glyphs, clusterBeg, glyphsList[i], ClusterStart);
  }
}

static const gunichar2 *
get_next_cluster(const gunichar2 *text,
                 gint             length,
                 gunichar2       *cluster,
                 gint            *numChars,
                 StateType       *clustState)
{
  const gunichar2 *p;
  gint            n_chars = 0;
  StateType       aSt = *clustState;

  p = text;
  while (p < text + length) {
    gunichar2 cur = *p;
    
    if ((n_chars == 0) ||
        ((n_chars > 0) && IsComposible(cluster[n_chars - 1], cur))) {
      cluster[n_chars++] = cur;
      aSt = SetClusterState(aSt, cur);
      p++;
    }
    else
      break;
  }
  
  *numChars = n_chars;
  *clustState = aSt;
  return p;
}

static void 
dvng_engine_shape(const char       *fontCharset,
                  const gunichar2  *text,
                  gint             length,
                  PangoliteAnalysis    *analysis,
                  PangoliteGlyphString *glyphs)
{
  DvngFontInfo    *fontInfo;
  const gunichar2 *p, *log_cluster;
  gunichar2       cluster[MAX_CLUSTER_CHRS];
  gint            num_chrs;
  StateType       aSt = St0;

  fontInfo = get_font_info(fontCharset);

  p = text;
  while (p < text + length) {
    log_cluster = p;
    aSt = St0;
    p = get_next_cluster(p, text + length - p, cluster, &num_chrs, &aSt);
    add_cluster(fontInfo, glyphs, log_cluster-text, cluster, num_chrs, &aSt);
  }
}

static PangoliteCoverage *
dvng_engine_get_coverage(const char *fontCharset,
                         const char *lang)
{
  PangoliteCoverage *result = pangolite_coverage_new ();  
  DvngFontInfo  *fontInfo = get_font_info (fontCharset);
  
  if (fontInfo->type != DVNG_FONT_NONE) {
    gunichar2 wc;
 
    for (wc = 0x901; wc <= 0x903; wc++)
      pangolite_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    for (wc = 0x905; wc <= 0x939; wc++)
      pangolite_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    for (wc = 0x93c; wc <= 0x94d; wc++)
      pangolite_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    for (wc = 0x950; wc <= 0x954; wc++)
      pangolite_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    for (wc = 0x958; wc <= 0x970; wc++)
      pangolite_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    /*    pangolite_coverage_set (result, ZWJ, PANGO_COVERAGE_EXACT); */
  }
  
  return result;
}

static PangoliteEngine *
dvng_engine_x_new ()
{
  PangoliteEngineShape *result;
  
  result = g_new (PangoliteEngineShape, 1);
  result->engine.id = SCRIPT_ENGINE_NAME;
  result->engine.type = PANGO_ENGINE_TYPE_SHAPE;
  result->engine.length = sizeof (result);
  result->script_shape = dvng_engine_shape;
  result->get_coverage = dvng_engine_get_coverage;
  return (PangoliteEngine *)result;
}

/* The following three functions provide the public module API for
 * Pangolite. If we are compiling it is a module, then we name the
 * entry points script_engine_list, etc. But if we are compiling
 * it for inclusion directly in Pangolite, then we need them to
 * to have distinct names for this module, so we prepend
 * _pangolite_thai_x_
 */
#ifdef X_MODULE_PREFIX
#define MODULE_ENTRY(func) _pangolite_dvng_x_##func
#else
#define MODULE_ENTRY(func) func
#endif

/* List the engines contained within this module
 */
void 
MODULE_ENTRY(script_engine_list) (PangoliteEngineInfo **engines, gint *n_engines)
{
  *engines = script_engines;
  *n_engines = G_N_ELEMENTS (script_engines);
}

/* Load a particular engine given the ID for the engine
 */
PangoliteEngine *
MODULE_ENTRY(script_engine_load) (const char *id)
{
  if (!strcmp (id, SCRIPT_ENGINE_NAME))
    return dvng_engine_x_new ();
  else
    return NULL;
}

void 
MODULE_ENTRY(script_engine_unload) (PangoliteEngine *engine)
{
}

