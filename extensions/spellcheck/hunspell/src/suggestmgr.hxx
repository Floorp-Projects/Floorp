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
/*
 * Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada
 * And Contributors.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All modifications to the source code must be clearly marked as
 *    such.  Binary redistributions based on modified source code
 *    must be clearly marked as modified versions in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY KEVIN B. HENDRICKS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * KEVIN B. HENDRICKS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SUGGESTMGR_HXX_
#define _SUGGESTMGR_HXX_

#define MAX_ROOTS 100
#define MAX_WORDS 100
#define MAX_GUESS 200
#define MAXNGRAMSUGS 4
#define MAXPHONSUGS 2
#define MAXCOMPOUNDSUGS 3

// timelimit: max ~1/4 sec (process time on Linux) for a time consuming function
#define TIMELIMIT (CLOCKS_PER_SEC >> 2)
#define MINTIMER 100
#define MAXPLUSTIMER 100

#define NGRAM_LONGER_WORSE (1 << 0)
#define NGRAM_ANY_MISMATCH (1 << 1)
#define NGRAM_LOWERING (1 << 2)
#define NGRAM_WEIGHTED (1 << 3)

#include "hunvisapi.h"

#include "atypes.hxx"
#include "affixmgr.hxx"
#include "hashmgr.hxx"
#include "langnum.hxx"
#include <time.h>

enum { LCS_UP, LCS_LEFT, LCS_UPLEFT };

class LIBHUNSPELL_DLL_EXPORTED SuggestMgr {
 private:
  SuggestMgr(const SuggestMgr&);
  SuggestMgr& operator=(const SuggestMgr&);

 private:
  char* ckey;
  int ckeyl;
  w_char* ckey_utf;

  char* ctry;
  int ctryl;
  w_char* ctry_utf;

  AffixMgr* pAMgr;
  int maxSug;
  struct cs_info* csconv;
  int utf8;
  int langnum;
  int nosplitsugs;
  int maxngramsugs;
  int maxcpdsugs;
  int complexprefixes;

 public:
  SuggestMgr(const char* tryme, int maxn, AffixMgr* aptr);
  ~SuggestMgr();

  int suggest(char*** slst, const char* word, int nsug, int* onlycmpdsug);
  int ngsuggest(char** wlst, const char* word, int ns, HashMgr** pHMgr, int md);
  int suggest_auto(char*** slst, const char* word, int nsug);
  int suggest_stems(char*** slst, const char* word, int nsug);
  int suggest_pos_stems(char*** slst, const char* word, int nsug);

  char* suggest_morph(const char* word);
  char* suggest_gen(char** pl, int pln, const char* pattern);
  char* suggest_morph_for_spelling_error(const char* word);

 private:
  int testsug(char** wlst,
              const char* candidate,
              int wl,
              int ns,
              int cpdsuggest,
              int* timer,
              clock_t* timelimit);
  int checkword(const char*, int, int, int*, clock_t*);
  int check_forbidden(const char*, int);

  int capchars(char**, const char*, int, int);
  int replchars(char**, const char*, int, int);
  int doubletwochars(char**, const char*, int, int);
  int forgotchar(char**, const char*, int, int);
  int swapchar(char**, const char*, int, int);
  int longswapchar(char**, const char*, int, int);
  int movechar(char**, const char*, int, int);
  int extrachar(char**, const char*, int, int);
  int badcharkey(char**, const char*, int, int);
  int badchar(char**, const char*, int, int);
  int twowords(char**, const char*, int, int);
  int fixstems(char**, const char*, int);

  int capchars_utf(char**, const w_char*, int wl, int, int);
  int doubletwochars_utf(char**, const w_char*, int wl, int, int);
  int forgotchar_utf(char**, const w_char*, int wl, int, int);
  int extrachar_utf(char**, const w_char*, int wl, int, int);
  int badcharkey_utf(char**, const w_char*, int wl, int, int);
  int badchar_utf(char**, const w_char*, int wl, int, int);
  int swapchar_utf(char**, const w_char*, int wl, int, int);
  int longswapchar_utf(char**, const w_char*, int, int, int);
  int movechar_utf(char**, const w_char*, int, int, int);

  int mapchars(char**, const char*, int, int);
  int map_related(const char*,
                  std::string&,
                  int,
                  char** wlst,
                  int,
                  int,
                  const mapentry*,
                  int,
                  int*,
                  clock_t*);
  int ngram(int n, const std::string& s1, const std::string& s2, int opt);
  int mystrlen(const char* word);
  int leftcommonsubstring(const char* s1, const char* s2);
  int commoncharacterpositions(const char* s1, const char* s2, int* is_swap);
  void bubblesort(char** rwd, char** rwd2, int* rsc, int n);
  void lcs(const char* s, const char* s2, int* l1, int* l2, char** result);
  int lcslen(const char* s, const char* s2);
  int lcslen(const std::string& s, const std::string& s2);
  char* suggest_hentry_gen(hentry* rv, const char* pattern);
};

#endif
