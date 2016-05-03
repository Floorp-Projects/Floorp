/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Hunspell, based on MySpell.
 *
 * The Initial Developers of the Original Code are
 * Kevin Hendricks (MySpell) and Németh László (Hunspell).
 * Portions created by the Initial Developers are Copyright (C) 2002-2005
 * the Initial Developers. All Rights Reserved.
 *
 * Contributor(s): David Einstein, Davide Prina, Giuseppe Modugno,
 * Gianluca Turconi, Simon Brouwer, Noll János, Bíró Árpád,
 * Goldman Eleonóra, Sarlós Tamás, Bencsáth Boldizsár, Halácsy Péter,
 * Dvornik László, Gefferth András, Nagy Viktor, Varga Dániel, Chris Halls,
 * Rene Engelhard, Bram Moolenaar, Dafydd Jones, Harri Pitkänen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _ATYPES_HXX_
#define _ATYPES_HXX_

#ifndef HUNSPELL_WARNING
#include <stdio.h>
#ifdef HUNSPELL_WARNING_ON
#define HUNSPELL_WARNING fprintf
#else
// empty inline function to switch off warnings (instead of the C99 standard
// variadic macros)
static inline void HUNSPELL_WARNING(FILE*, const char*, ...) {}
#endif
#endif

// HUNSTEM def.
#define HUNSTEM

#include "hashmgr.hxx"
#include "w_char.hxx"
#include <algorithm>
#include <string>

#define SETSIZE 256
#define CONTSIZE 65536

// affentry options
#define aeXPRODUCT (1 << 0)
#define aeUTF8 (1 << 1)
#define aeALIASF (1 << 2)
#define aeALIASM (1 << 3)
#define aeLONGCOND (1 << 4)

// compound options
#define IN_CPD_NOT 0
#define IN_CPD_BEGIN 1
#define IN_CPD_END 2
#define IN_CPD_OTHER 3

// info options
#define SPELL_COMPOUND (1 << 0)
#define SPELL_FORBIDDEN (1 << 1)
#define SPELL_ALLCAP (1 << 2)
#define SPELL_NOCAP (1 << 3)
#define SPELL_INITCAP (1 << 4)
#define SPELL_ORIGCAP (1 << 5)
#define SPELL_WARN (1 << 6)

#define MAXLNLEN 8192

#define MINCPDLEN 3
#define MAXCOMPOUND 10
#define MAXCONDLEN 20
#define MAXCONDLEN_1 (MAXCONDLEN - sizeof(char*))

#define MAXACC 1000

#define FLAG unsigned short
#define FLAG_NULL 0x00
#define FREE_FLAG(a) a = 0

#define TESTAFF(a, b, c) (std::binary_search(a, a + c, b))

struct affentry {
  std::string strip;
  std::string appnd;
  char numconds;
  char opts;
  unsigned short aflag;
  unsigned short* contclass;
  short contclasslen;
  union {
    char conds[MAXCONDLEN];
    struct {
      char conds1[MAXCONDLEN_1];
      char* conds2;
    } l;
  } c;
  char* morphcode;
};

struct guessword {
  char* word;
  bool allow;
  char* orig;
};

struct mapentry {
  char** set;
  int len;
};

struct flagentry {
  FLAG* def;
  int len;
};

struct patentry {
  char* pattern;
  char* pattern2;
  char* pattern3;
  FLAG cond;
  FLAG cond2;
};

#endif
