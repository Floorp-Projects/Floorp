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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "suggestmgr.hxx"
#include "htypes.hxx"
#include "csutil.hxx"

const w_char W_VLINE = {'\0', '|'};

SuggestMgr::SuggestMgr(const char* tryme, int maxn, AffixMgr* aptr) {
  // register affix manager and check in string of chars to
  // try when building candidate suggestions
  pAMgr = aptr;

  csconv = NULL;

  ckeyl = 0;
  ckey = NULL;
  ckey_utf = NULL;

  ctryl = 0;
  ctry = NULL;
  ctry_utf = NULL;

  utf8 = 0;
  langnum = 0;
  complexprefixes = 0;

  maxSug = maxn;
  nosplitsugs = 0;
  maxngramsugs = MAXNGRAMSUGS;
  maxcpdsugs = MAXCOMPOUNDSUGS;

  if (pAMgr) {
    langnum = pAMgr->get_langnum();
    ckey = pAMgr->get_key_string();
    nosplitsugs = pAMgr->get_nosplitsugs();
    if (pAMgr->get_maxngramsugs() >= 0)
      maxngramsugs = pAMgr->get_maxngramsugs();
    utf8 = pAMgr->get_utf8();
    if (pAMgr->get_maxcpdsugs() >= 0)
      maxcpdsugs = pAMgr->get_maxcpdsugs();
    if (!utf8) {
      char* enc = pAMgr->get_encoding();
      csconv = get_current_cs(enc);
      free(enc);
    }
    complexprefixes = pAMgr->get_complexprefixes();
  }

  if (ckey) {
    if (utf8) {
      std::vector<w_char> t;
      ckeyl = u8_u16(t, ckey);
      ckey_utf = (w_char*)malloc(ckeyl * sizeof(w_char));
      if (ckey_utf)
        memcpy(ckey_utf, &t[0], ckeyl * sizeof(w_char));
      else
        ckeyl = 0;
    } else {
      ckeyl = strlen(ckey);
    }
  }

  if (tryme) {
    ctry = mystrdup(tryme);
    if (ctry)
      ctryl = strlen(ctry);
    if (ctry && utf8) {
      std::vector<w_char> t;
      ctryl = u8_u16(t, tryme);
      ctry_utf = (w_char*)malloc(ctryl * sizeof(w_char));
      if (ctry_utf)
        memcpy(ctry_utf, &t[0], ctryl * sizeof(w_char));
      else
        ctryl = 0;
    }
  }
}

SuggestMgr::~SuggestMgr() {
  pAMgr = NULL;
  if (ckey)
    free(ckey);
  ckey = NULL;
  if (ckey_utf)
    free(ckey_utf);
  ckey_utf = NULL;
  ckeyl = 0;
  if (ctry)
    free(ctry);
  ctry = NULL;
  if (ctry_utf)
    free(ctry_utf);
  ctry_utf = NULL;
  ctryl = 0;
  maxSug = 0;
#ifdef MOZILLA_CLIENT
  delete[] csconv;
#endif
}

int SuggestMgr::testsug(char** wlst,
                        const char* candidate,
                        int wl,
                        int ns,
                        int cpdsuggest,
                        int* timer,
                        clock_t* timelimit) {
  int cwrd = 1;
  if (ns == maxSug)
    return maxSug;
  for (int k = 0; k < ns; k++) {
    if (strcmp(candidate, wlst[k]) == 0) {
      cwrd = 0;
      break;
    }
  }
  if ((cwrd) && checkword(candidate, wl, cpdsuggest, timer, timelimit)) {
    wlst[ns] = mystrdup(candidate);
    if (wlst[ns] == NULL) {
      for (int j = 0; j < ns; j++)
        free(wlst[j]);
      return -1;
    }
    ns++;
  }
  return ns;
}

// generate suggestions for a misspelled word
//    pass in address of array of char * pointers
// onlycompoundsug: probably bad suggestions (need for ngram sugs, too)

int SuggestMgr::suggest(char*** slst,
                        const char* w,
                        int nsug,
                        int* onlycompoundsug) {
  int nocompoundtwowords = 0;
  char** wlst;
  std::vector<w_char> word_utf;
  int wl = 0;
  int nsugorig = nsug;
  std::string w2;
  const char* word = w;
  int oldSug = 0;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    w2.assign(w);
    if (utf8)
      reverseword_utf(w2);
    else
      reverseword(w2);
    word = w2.c_str();
  }

  if (*slst) {
    wlst = *slst;
  } else {
    wlst = (char**)malloc(maxSug * sizeof(char*));
    if (wlst == NULL)
      return -1;
    for (int i = 0; i < maxSug; i++) {
      wlst[i] = NULL;
    }
  }

  if (utf8) {
    wl = u8_u16(word_utf, word);
    if (wl == -1) {
      *slst = wlst;
      return nsug;
    }
  }

  for (int cpdsuggest = 0; (cpdsuggest < 2) && (nocompoundtwowords == 0);
       cpdsuggest++) {
    // limit compound suggestion
    if (cpdsuggest > 0)
      oldSug = nsug;

    // suggestions for an uppercase word (html -> HTML)
    if ((nsug < maxSug) && (nsug > -1)) {
      nsug = (utf8) ? capchars_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : capchars(wlst, word, nsug, cpdsuggest);
    }

    // perhaps we made a typical fault of spelling
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = replchars(wlst, word, nsug, cpdsuggest);
    }

    // perhaps we made chose the wrong char from a related set
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = mapchars(wlst, word, nsug, cpdsuggest);
    }

    // only suggest compound words when no other suggestion
    if ((cpdsuggest == 0) && (nsug > nsugorig))
      nocompoundtwowords = 1;

    // did we swap the order of chars by mistake
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? swapchar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : swapchar(wlst, word, nsug, cpdsuggest);
    }

    // did we swap the order of non adjacent chars by mistake
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? longswapchar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : longswapchar(wlst, word, nsug, cpdsuggest);
    }

    // did we just hit the wrong key in place of a good char (case and keyboard)
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? badcharkey_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : badcharkey(wlst, word, nsug, cpdsuggest);
    }

    // did we add a char that should not be there
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? extrachar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : extrachar(wlst, word, nsug, cpdsuggest);
    }

    // did we forgot a char
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? forgotchar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : forgotchar(wlst, word, nsug, cpdsuggest);
    }

    // did we move a char
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? movechar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : movechar(wlst, word, nsug, cpdsuggest);
    }

    // did we just hit the wrong key in place of a good char
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? badchar_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : badchar(wlst, word, nsug, cpdsuggest);
    }

    // did we double two characters
    if ((nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = (utf8) ? doubletwochars_utf(wlst, &word_utf[0], wl, nsug, cpdsuggest)
                    : doubletwochars(wlst, word, nsug, cpdsuggest);
    }

    // perhaps we forgot to hit space and two words ran together
    if (!nosplitsugs && (nsug < maxSug) && (nsug > -1) &&
        (!cpdsuggest || (nsug < oldSug + maxcpdsugs))) {
      nsug = twowords(wlst, word, nsug, cpdsuggest);
    }

  }  // repeating ``for'' statement compounding support

  if (nsug < 0) {
    // we ran out of memory - we should free up as much as possible
    for (int i = 0; i < maxSug; i++)
      if (wlst[i] != NULL)
        free(wlst[i]);
    free(wlst);
    wlst = NULL;
  }

  if (!nocompoundtwowords && (nsug > 0) && onlycompoundsug)
    *onlycompoundsug = 1;

  *slst = wlst;
  return nsug;
}

// suggestions for an uppercase word (html -> HTML)
int SuggestMgr::capchars_utf(char** wlst,
                             const w_char* word,
                             int wl,
                             int ns,
                             int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  mkallcap_utf(candidate_utf, langnum);
  std::string candidate;
  u16_u8(candidate, candidate_utf);
  return testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                 NULL);
}

// suggestions for an uppercase word (html -> HTML)
int SuggestMgr::capchars(char** wlst,
                         const char* word,
                         int ns,
                         int cpdsuggest) {
  std::string candidate(word);
  mkallcap(candidate, csconv);
  return testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                 NULL);
}

// suggestions for when chose the wrong char out of a related set
int SuggestMgr::mapchars(char** wlst,
                         const char* word,
                         int ns,
                         int cpdsuggest) {
  std::string candidate;
  clock_t timelimit;
  int timer;

  int wl = strlen(word);
  if (wl < 2 || !pAMgr)
    return ns;

  int nummap = pAMgr->get_nummap();
  struct mapentry* maptable = pAMgr->get_maptable();
  if (maptable == NULL)
    return ns;

  timelimit = clock();
  timer = MINTIMER;
  return map_related(word, candidate, 0, wlst, cpdsuggest, ns,
                     maptable, nummap, &timer, &timelimit);
}

int SuggestMgr::map_related(const char* word,
                            std::string& candidate,
                            int wn,
                            char** wlst,
                            int cpdsuggest,
                            int ns,
                            const mapentry* maptable,
                            int nummap,
                            int* timer,
                            clock_t* timelimit) {
  if (*(word + wn) == '\0') {
    int cwrd = 1;
    for (int m = 0; m < ns; m++) {
      if (candidate == wlst[m]) {
        cwrd = 0;
        break;
      }
    }
    if ((cwrd) && checkword(candidate.c_str(), candidate.size(), cpdsuggest, timer, timelimit)) {
      if (ns < maxSug) {
        wlst[ns] = mystrdup(candidate.c_str());
        if (wlst[ns] == NULL)
          return -1;
        ns++;
      }
    }
    return ns;
  }
  int in_map = 0;
  for (int j = 0; j < nummap; j++) {
    for (int k = 0; k < maptable[j].len; k++) {
      int len = strlen(maptable[j].set[k]);
      if (strncmp(maptable[j].set[k], word + wn, len) == 0) {
        in_map = 1;
        size_t cn = candidate.size();
        for (int l = 0; l < maptable[j].len; l++) {
          candidate.resize(cn);
          candidate.append(maptable[j].set[l]);
          ns = map_related(word, candidate, wn + len, wlst,
                           cpdsuggest, ns, maptable, nummap, timer, timelimit);
          if (!(*timer))
            return ns;
        }
      }
    }
  }
  if (!in_map) {
    candidate.push_back(*(word + wn));
    ns = map_related(word, candidate, wn + 1, wlst, cpdsuggest, ns,
                     maptable, nummap, timer, timelimit);
  }
  return ns;
}

// suggestions for a typical fault of spelling, that
// differs with more, than 1 letter from the right form.
int SuggestMgr::replchars(char** wlst,
                          const char* word,
                          int ns,
                          int cpdsuggest) {
  std::string candidate;
  int wl = strlen(word);
  if (wl < 2 || !pAMgr)
    return ns;
  int numrep = pAMgr->get_numrep();
  struct replentry* reptable = pAMgr->get_reptable();
  if (reptable == NULL)
    return ns;
  for (int i = 0; i < numrep; i++) {
    const char* r = word;
    // search every occurence of the pattern in the word
    while ((r = strstr(r, reptable[i].pattern)) != NULL &&
           (!reptable[i].end || strlen(r) == strlen(reptable[i].pattern)) &&
           (!reptable[i].start || r == word)) {
      candidate.assign(word);
      candidate.resize(r - word);
      candidate.append(reptable[i].pattern2);
      int lenp = strlen(reptable[i].pattern);
      candidate.append(r + lenp);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                   NULL);
      if (ns == -1)
        return -1;
      // check REP suggestions with space
      size_t sp = candidate.find(' ');
      if (sp != std::string::npos) {
        size_t prev = 0;
        while (sp != std::string::npos) {
          std::string prev_chunk = candidate.substr(prev, sp - prev);
          if (checkword(prev_chunk.c_str(), prev_chunk.size(), 0, NULL, NULL)) {
            int oldns = ns;
            std::string post_chunk = candidate.substr(sp + 1);
            ns = testsug(wlst, post_chunk.c_str(), post_chunk.size(), ns, cpdsuggest, NULL,
                         NULL);
            if (ns == -1)
              return -1;
            if (oldns < ns) {
              free(wlst[ns - 1]);
              wlst[ns - 1] = mystrdup(candidate.c_str());
              if (!wlst[ns - 1])
                return -1;
            }
          }
          prev = sp + 1;
          sp = candidate.find(' ', prev);
        }
      }
      r++;  // search for the next letter
    }
  }
  return ns;
}

// perhaps we doubled two characters (pattern aba -> ababa, for example vacation
// -> vacacation)
int SuggestMgr::doubletwochars(char** wlst,
                               const char* word,
                               int ns,
                               int cpdsuggest) {
  int state = 0;
  int wl = strlen(word);
  if (wl < 5 || !pAMgr)
    return ns;
  for (int i = 2; i < wl; i++) {
    if (word[i] == word[i - 2]) {
      state++;
      if (state == 3) {
        std::string candidate(word, word + i - 1);
        candidate.insert(candidate.end(), word + i + 1, word + wl);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
        if (ns == -1)
          return -1;
        state = 0;
      }
    } else {
      state = 0;
    }
  }
  return ns;
}

// perhaps we doubled two characters (pattern aba -> ababa, for example vacation
// -> vacacation)
int SuggestMgr::doubletwochars_utf(char** wlst,
                                   const w_char* word,
                                   int wl,
                                   int ns,
                                   int cpdsuggest) {
  int state = 0;
  if (wl < 5 || !pAMgr)
    return ns;
  for (int i = 2; i < wl; i++) {
    if (word[i] == word[i - 2]) {
      state++;
      if (state == 3) {
        std::vector<w_char> candidate_utf(word, word + i - 1);
        candidate_utf.insert(candidate_utf.end(), word + i + 1, word + wl);
        std::string candidate;
        u16_u8(candidate, candidate_utf);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                     NULL);
        if (ns == -1)
          return -1;
        state = 0;
      }
    } else {
      state = 0;
    }
  }
  return ns;
}

// error is wrong char in place of correct one (case and keyboard related
// version)
int SuggestMgr::badcharkey(char** wlst,
                           const char* word,
                           int ns,
                           int cpdsuggest) {
  std::string candidate(word);

  // swap out each char one by one and try uppercase and neighbor
  // keyboard chars in its place to see if that makes a good word
  for (size_t i = 0; i < candidate.size(); ++i) {
    char tmpc = candidate[i];
    // check with uppercase letters
    candidate[i] = csconv[((unsigned char)tmpc)].cupper;
    if (tmpc != candidate[i]) {
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
      if (ns == -1)
        return -1;
      candidate[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey)
      continue;
    char* loc = strchr(ckey, tmpc);
    while (loc) {
      if ((loc > ckey) && (*(loc - 1) != '|')) {
        candidate[i] = *(loc - 1);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
        if (ns == -1)
          return -1;
      }
      if ((*(loc + 1) != '|') && (*(loc + 1) != '\0')) {
        candidate[i] = *(loc + 1);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
        if (ns == -1)
          return -1;
      }
      loc = strchr(loc + 1, tmpc);
    }
    candidate[i] = tmpc;
  }
  return ns;
}

// error is wrong char in place of correct one (case and keyboard related
// version)
int SuggestMgr::badcharkey_utf(char** wlst,
                               const w_char* word,
                               int wl,
                               int ns,
                               int cpdsuggest) {
  std::string candidate;
  std::vector<w_char> candidate_utf(word, word + wl);
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i = 0; i < wl; i++) {
    w_char tmpc = candidate_utf[i];
    // check with uppercase letters
    candidate_utf[i] = upper_utf(candidate_utf[i], 1);
    if (tmpc != candidate_utf[i]) {
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                   NULL);
      if (ns == -1)
        return -1;
      candidate_utf[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey)
      continue;
    w_char* loc = ckey_utf;
    while ((loc < (ckey_utf + ckeyl)) && *loc != tmpc)
      loc++;
    while (loc < (ckey_utf + ckeyl)) {
      if ((loc > ckey_utf) && *(loc - 1) != W_VLINE) {
        candidate_utf[i] = *(loc - 1);
        u16_u8(candidate, candidate_utf);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                     NULL);
        if (ns == -1)
          return -1;
      }
      if (((loc + 1) < (ckey_utf + ckeyl)) && (*(loc + 1) != W_VLINE)) {
        candidate_utf[i] = *(loc + 1);
        u16_u8(candidate, candidate_utf);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                     NULL);
        if (ns == -1)
          return -1;
      }
      do {
        loc++;
      } while ((loc < (ckey_utf + ckeyl)) && *loc != tmpc);
    }
    candidate_utf[i] = tmpc;
  }
  return ns;
}

// error is wrong char in place of correct one
int SuggestMgr::badchar(char** wlst, const char* word, int ns, int cpdsuggest) {
  std::string candidate(word);
  clock_t timelimit = clock();
  int timer = MINTIMER;
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int j = 0; j < ctryl; j++) {
    for (std::string::reverse_iterator aI = candidate.rbegin(), aEnd = candidate.rend(); aI != aEnd; ++aI) {
      char tmpc = *aI;
      if (ctry[j] == tmpc)
        continue;
      *aI = ctry[j];
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, &timer, &timelimit);
      if (ns == -1)
        return -1;
      if (!timer)
        return ns;
      *aI = tmpc;
    }
  }
  return ns;
}

// error is wrong char in place of correct one
int SuggestMgr::badchar_utf(char** wlst,
                            const w_char* word,
                            int wl,
                            int ns,
                            int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  std::string candidate;
  clock_t timelimit = clock();
  int timer = MINTIMER;
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int j = 0; j < ctryl; j++) {
    for (int i = wl - 1; i >= 0; i--) {
      w_char tmpc = candidate_utf[i];
      if (tmpc == ctry_utf[j])
        continue;
      candidate_utf[i] = ctry_utf[j];
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, &timer,
                   &timelimit);
      if (ns == -1)
        return -1;
      if (!timer)
        return ns;
      candidate_utf[i] = tmpc;
    }
  }
  return ns;
}

// error is word has an extra letter it does not need
int SuggestMgr::extrachar_utf(char** wlst,
                              const w_char* word,
                              int wl,
                              int ns,
                              int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return ns;
  // try omitting one char of word at a time
  for (size_t i = 0; i < candidate_utf.size(); ++i) {
    size_t index = candidate_utf.size() - 1 - i;
    w_char tmpc = candidate_utf[index];
    candidate_utf.erase(candidate_utf.begin() + index);
    std::string candidate;
    u16_u8(candidate, candidate_utf);
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    candidate_utf.insert(candidate_utf.begin() + index, tmpc);
  }
  return ns;
}

// error is word has an extra letter it does not need
int SuggestMgr::extrachar(char** wlst,
                          const char* word,
                          int ns,
                          int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return ns;
  // try omitting one char of word at a time
  for (size_t i = 0; i < candidate.size(); ++i) {
    size_t index = candidate.size() - 1 - i;
    char tmpc = candidate[index];
    candidate.erase(candidate.begin() + index);
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    candidate.insert(candidate.begin() + index, tmpc);
  }
  return ns;
}

// error is missing a letter it needs
int SuggestMgr::forgotchar(char** wlst,
                           const char* word,
                           int ns,
                           int cpdsuggest) {
  std::string candidate(word);
  clock_t timelimit = clock();
  int timer = MINTIMER;

  // try inserting a tryme character before every letter (and the null
  // terminator)
  for (int k = 0; k < ctryl; ++k) {
    for (size_t i = 0; i <= candidate.size(); ++i) {
      size_t index = candidate.size() - i;
      candidate.insert(candidate.begin() + index, ctry[k]);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, &timer, &timelimit);
      if (ns == -1)
        return -1;
      if (!timer)
        return ns;
      candidate.erase(candidate.begin() + index);
    }
  }
  return ns;
}

// error is missing a letter it needs
int SuggestMgr::forgotchar_utf(char** wlst,
                               const w_char* word,
                               int wl,
                               int ns,
                               int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  clock_t timelimit = clock();
  int timer = MINTIMER;

  // try inserting a tryme character at the end of the word and before every
  // letter
  for (int k = 0; k < ctryl; ++k) {
    for (size_t i = 0; i <= candidate_utf.size(); ++i) {
      size_t index = candidate_utf.size() - i;
      candidate_utf.insert(candidate_utf.begin() + index, ctry_utf[k]);
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, &timer,
                   &timelimit);
      if (ns == -1)
        return -1;
      if (!timer)
        return ns;
      candidate_utf.erase(candidate_utf.begin() + index);
    }
  }
  return ns;
}

/* error is should have been two words */
int SuggestMgr::twowords(char** wlst,
                         const char* word,
                         int ns,
                         int cpdsuggest) {
  int c1, c2;
  int forbidden = 0;
  int cwrd;

  int wl = strlen(word);
  if (wl < 3)
    return ns;

  if (langnum == LANG_hu)
    forbidden = check_forbidden(word, wl);

  char* candidate = (char*)malloc(wl + 2);
  strcpy(candidate + 1, word);

  // split the string into two pieces after every char
  // if both pieces are good words make them a suggestion
  for (char* p = candidate + 1; p[1] != '\0'; p++) {
    p[-1] = *p;
    // go to end of the UTF-8 character
    while (utf8 && ((p[1] & 0xc0) == 0x80)) {
      *p = p[1];
      p++;
    }
    if (utf8 && p[1] == '\0')
      break;  // last UTF-8 character
    *p = '\0';
    c1 = checkword(candidate, strlen(candidate), cpdsuggest, NULL, NULL);
    if (c1) {
      c2 = checkword((p + 1), strlen(p + 1), cpdsuggest, NULL, NULL);
      if (c2) {
        *p = ' ';

        // spec. Hungarian code (need a better compound word support)
        if ((langnum == LANG_hu) && !forbidden &&
            // if 3 repeating letter, use - instead of space
            (((p[-1] == p[1]) &&
              (((p > candidate + 1) && (p[-1] == p[-2])) || (p[-1] == p[2]))) ||
             // or multiple compounding, with more, than 6 syllables
             ((c1 == 3) && (c2 >= 2))))
          *p = '-';

        cwrd = 1;
        for (int k = 0; k < ns; k++) {
          if (strcmp(candidate, wlst[k]) == 0) {
            cwrd = 0;
            break;
          }
        }
        if (ns < maxSug) {
          if (cwrd) {
            wlst[ns] = mystrdup(candidate);
            if (wlst[ns] == NULL) {
              free(candidate);
              return -1;
            }
            ns++;
          }
        } else {
          free(candidate);
          return ns;
        }
        // add two word suggestion with dash, if TRY string contains
        // "a" or "-"
        // NOTE: cwrd doesn't modified for REP twoword sugg.
        if (ctry && (strchr(ctry, 'a') || strchr(ctry, '-')) &&
            mystrlen(p + 1) > 1 && mystrlen(candidate) - mystrlen(p) > 1) {
          *p = '-';
          for (int k = 0; k < ns; k++) {
            if (strcmp(candidate, wlst[k]) == 0) {
              cwrd = 0;
              break;
            }
          }
          if (ns < maxSug) {
            if (cwrd) {
              wlst[ns] = mystrdup(candidate);
              if (wlst[ns] == NULL) {
                free(candidate);
                return -1;
              }
              ns++;
            }
          } else {
            free(candidate);
            return ns;
          }
        }
      }
    }
  }
  free(candidate);
  return ns;
}

// error is adjacent letter were swapped
int SuggestMgr::swapchar(char** wlst,
                         const char* word,
                         int ns,
                         int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return ns;

  // try swapping adjacent chars one by one
  for (size_t i = 0; i < candidate.size() - 1; ++i) {
    std::swap(candidate[i], candidate[i+1]);
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    std::swap(candidate[i], candidate[i+1]);
  }

  // try double swaps for short words
  // ahev -> have, owudl -> would
  if (candidate.size() == 4 || candidate.size() == 5) {
    candidate[0] = word[1];
    candidate[1] = word[0];
    candidate[2] = word[2];
    candidate[candidate.size() - 2] = word[candidate.size() - 1];
    candidate[candidate.size() - 1] = word[candidate.size() - 2];
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    if (candidate.size() == 5) {
      candidate[0] = word[0];
      candidate[1] = word[2];
      candidate[2] = word[1];
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
      if (ns == -1)
        return -1;
    }
  }

  return ns;
}

// error is adjacent letter were swapped
int SuggestMgr::swapchar_utf(char** wlst,
                             const w_char* word,
                             int wl,
                             int ns,
                             int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return ns;

  std::string candidate;
  // try swapping adjacent chars one by one
  for (size_t i = 0; i < candidate_utf.size() - 1; ++i) {
    std::swap(candidate_utf[i], candidate_utf[i+1]);
    u16_u8(candidate, candidate_utf);
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    std::swap(candidate_utf[i], candidate_utf[i+1]);
  }

  // try double swaps for short words
  // ahev -> have, owudl -> would, suodn -> sound
  if (candidate_utf.size() == 4 || candidate_utf.size() == 5) {
    candidate_utf[0] = word[1];
    candidate_utf[1] = word[0];
    candidate_utf[2] = word[2];
    candidate_utf[candidate_utf.size() - 2] = word[candidate_utf.size() - 1];
    candidate_utf[candidate_utf.size() - 1] = word[candidate_utf.size() - 2];
    u16_u8(candidate, candidate_utf);
    ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
    if (ns == -1)
      return -1;
    if (candidate_utf.size() == 5) {
      candidate_utf[0] = word[0];
      candidate_utf[1] = word[2];
      candidate_utf[2] = word[1];
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
      if (ns == -1)
        return -1;
    }
  }
  return ns;
}

// error is not adjacent letter were swapped
int SuggestMgr::longswapchar(char** wlst,
                             const char* word,
                             int ns,
                             int cpdsuggest) {
  std::string candidate(word);
  // try swapping not adjacent chars one by one
  for (std::string::iterator p = candidate.begin(); p < candidate.end(); ++p) {
    for (std::string::iterator q = candidate.begin(); q < candidate.end(); ++q) {
      if (abs(std::distance(q, p)) > 1) {
        std::swap(*p, *q);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
        if (ns == -1)
          return -1;
        std::swap(*p, *q);
      }
    }
  }
  return ns;
}

// error is adjacent letter were swapped
int SuggestMgr::longswapchar_utf(char** wlst,
                                 const w_char* word,
                                 int wl,
                                 int ns,
                                 int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  // try swapping not adjacent chars
  for (std::vector<w_char>::iterator p = candidate_utf.begin(); p < candidate_utf.end(); ++p) {
    for (std::vector<w_char>::iterator q = candidate_utf.begin(); q < candidate_utf.end(); ++q) {
      if (abs(std::distance(q, p)) > 1) {
        std::swap(*p, *q);
        std::string candidate;
        u16_u8(candidate, candidate_utf);
        ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                     NULL);
        if (ns == -1)
          return -1;
        std::swap(*p, *q);
      }
    }
  }
  return ns;
}

// error is a letter was moved
int SuggestMgr::movechar(char** wlst,
                         const char* word,
                         int ns,
                         int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return ns;

  // try moving a char
  for (std::string::iterator p = candidate.begin(); p < candidate.end(); ++p) {
    for (std::string::iterator q = p + 1; q < candidate.end() && std::distance(p, q) < 10; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
      if (ns == -1)
        return -1;
    }
    std::copy(word, word + candidate.size(), candidate.begin());
  }

  for (std::string::reverse_iterator p = candidate.rbegin(), pEnd = candidate.rend() - 1; p != pEnd; ++p) {
    for (std::string::reverse_iterator q = p + 1, qEnd = candidate.rend(); q != qEnd && std::distance(p, q) < 10; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL, NULL);
      if (ns == -1)
        return -1;
    }
    std::copy(word, word + candidate.size(), candidate.begin());
  }

  return ns;
}

// error is a letter was moved
int SuggestMgr::movechar_utf(char** wlst,
                             const w_char* word,
                             int wl,
                             int ns,
                             int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return ns;

  // try moving a char
  for (std::vector<w_char>::iterator p = candidate_utf.begin(); p < candidate_utf.end(); ++p) {
    for (std::vector<w_char>::iterator q = p + 1; q < candidate_utf.end() && std::distance(p, q) < 10; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                   NULL);
      if (ns == -1)
        return -1;
    }
    std::copy(word, word + candidate_utf.size(), candidate_utf.begin());
  }

  for (std::vector<w_char>::iterator p = candidate_utf.begin() + candidate_utf.size() - 1; p > candidate_utf.begin(); --p) {
    for (std::vector<w_char>::iterator q = p - 1; q >= candidate_utf.begin() && std::distance(q, p) < 10; --q) {
      std::swap(*q, *(q + 1));
      if (std::distance(q, p) < 2)
        continue;  // omit swap char
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      ns = testsug(wlst, candidate.c_str(), candidate.size(), ns, cpdsuggest, NULL,
                   NULL);
      if (ns == -1)
        return -1;
    }
    std::copy(word, word + candidate_utf.size(), candidate_utf.begin());
  }

  return ns;
}

// generate a set of suggestions for very poorly spelled words
int SuggestMgr::ngsuggest(char** wlst,
                          const char* w,
                          int ns,
                          HashMgr** pHMgr,
                          int md) {
  int i, j;
  int lval;
  int sc;
  int lp, lpphon;
  int nonbmp = 0;

  // exhaustively search through all root words
  // keeping track of the MAX_ROOTS most similar root words
  struct hentry* roots[MAX_ROOTS];
  char* rootsphon[MAX_ROOTS];
  int scores[MAX_ROOTS];
  int scoresphon[MAX_ROOTS];
  for (i = 0; i < MAX_ROOTS; i++) {
    roots[i] = NULL;
    scores[i] = -100 * i;
    rootsphon[i] = NULL;
    scoresphon[i] = -100 * i;
  }
  lp = MAX_ROOTS - 1;
  lpphon = MAX_ROOTS - 1;
  int low = NGRAM_LOWERING;

  std::string w2;
  const char* word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    w2.assign(w);
    if (utf8)
      reverseword_utf(w2);
    else
      reverseword(w2);
    word = w2.c_str();
  }

  std::vector<w_char> u8;
  int nc = strlen(word);
  int n = (utf8) ? u8_u16(u8, word) : nc;

  // set character based ngram suggestion for words with non-BMP Unicode
  // characters
  if (n == -1) {
    utf8 = 0;  // XXX not state-free
    n = nc;
    nonbmp = 1;
    low = 0;
  }

  struct hentry* hp = NULL;
  int col = -1;
  phonetable* ph = (pAMgr) ? pAMgr->get_phonetable() : NULL;
  std::string target;
  std::string candidate;
  if (ph) {
    if (utf8) {
      std::vector<w_char> _w;
      u8_u16(_w, word);
      mkallcap_utf(_w, langnum);
      u16_u8(candidate, _w);
    } else {
      candidate.assign(word);
      if (!nonbmp)
        mkallcap(candidate, csconv);
    }
    target = phonet(candidate, *ph);  // XXX phonet() is 8-bit (nc, not n)
  }

  FLAG forbiddenword = pAMgr ? pAMgr->get_forbiddenword() : FLAG_NULL;
  FLAG nosuggest = pAMgr ? pAMgr->get_nosuggest() : FLAG_NULL;
  FLAG nongramsuggest = pAMgr ? pAMgr->get_nongramsuggest() : FLAG_NULL;
  FLAG onlyincompound = pAMgr ? pAMgr->get_onlyincompound() : FLAG_NULL;

  for (i = 0; i < md; i++) {
    while (0 != (hp = (pHMgr[i])->walk_hashtable(col, hp))) {
      if ((hp->astr) && (pAMgr) &&
          (TESTAFF(hp->astr, forbiddenword, hp->alen) ||
           TESTAFF(hp->astr, ONLYUPCASEFLAG, hp->alen) ||
           TESTAFF(hp->astr, nosuggest, hp->alen) ||
           TESTAFF(hp->astr, nongramsuggest, hp->alen) ||
           TESTAFF(hp->astr, onlyincompound, hp->alen)))
        continue;

      sc = ngram(3, word, HENTRY_WORD(hp), NGRAM_LONGER_WORSE + low) +
           leftcommonsubstring(word, HENTRY_WORD(hp));

      // check special pronounciation
      std::string f;
      if ((hp->var & H_OPT_PHON) &&
          copy_field(f, HENTRY_DATA(hp), MORPH_PHON)) {
        int sc2 = ngram(3, word, f, NGRAM_LONGER_WORSE + low) +
                  +leftcommonsubstring(word, f.c_str());
        if (sc2 > sc)
          sc = sc2;
      }

      int scphon = -20000;
      if (ph && (sc > 2) && (abs(n - (int)hp->clen) <= 3)) {
        if (utf8) {
          std::vector<w_char> _w;
          u8_u16(_w, HENTRY_WORD(hp));
          mkallcap_utf(_w, langnum);
          u16_u8(candidate, _w);
        } else {
          candidate.assign(HENTRY_WORD(hp));
          mkallcap(candidate, csconv);
        }
        std::string target2 = phonet(candidate, *ph);
        scphon = 2 * ngram(3, target, target2, NGRAM_LONGER_WORSE);
      }

      if (sc > scores[lp]) {
        scores[lp] = sc;
        roots[lp] = hp;
        lval = sc;
        for (j = 0; j < MAX_ROOTS; j++)
          if (scores[j] < lval) {
            lp = j;
            lval = scores[j];
          }
      }

      if (scphon > scoresphon[lpphon]) {
        scoresphon[lpphon] = scphon;
        rootsphon[lpphon] = HENTRY_WORD(hp);
        lval = scphon;
        for (j = 0; j < MAX_ROOTS; j++)
          if (scoresphon[j] < lval) {
            lpphon = j;
            lval = scoresphon[j];
          }
      }
    }
  }

  // find minimum threshold for a passable suggestion
  // mangle original word three differnt ways
  // and score them to generate a minimum acceptable score
  int thresh = 0;
  for (int sp = 1; sp < 4; sp++) {
    if (utf8) {
      for (int k = sp; k < n; k += 4) {
        u8[k].l = '*';
        u8[k].h = 0;
      }
      std::string mw;
      u16_u8(mw, u8);
      thresh = thresh + ngram(n, word, mw, NGRAM_ANY_MISMATCH + low);
    } else {
      std::string mw(word);
      for (int k = sp; k < n; k += 4)
        mw[k] = '*';
      thresh = thresh + ngram(n, word, mw, NGRAM_ANY_MISMATCH + low);
    }
  }
  thresh = thresh / 3;
  thresh--;

  // now expand affixes on each of these root words and
  // and use length adjusted ngram scores to select
  // possible suggestions
  char* guess[MAX_GUESS];
  char* guessorig[MAX_GUESS];
  int gscore[MAX_GUESS];
  for (i = 0; i < MAX_GUESS; i++) {
    guess[i] = NULL;
    guessorig[i] = NULL;
    gscore[i] = -100 * i;
  }

  lp = MAX_GUESS - 1;

  struct guessword* glst;
  glst = (struct guessword*)calloc(MAX_WORDS, sizeof(struct guessword));
  if (!glst) {
    if (nonbmp)
      utf8 = 1;
    return ns;
  }

  for (i = 0; i < MAX_ROOTS; i++) {
    if (roots[i]) {
      struct hentry* rp = roots[i];

      std::string f;
      const char *field = NULL;
      if ((rp->var & H_OPT_PHON) && copy_field(f, HENTRY_DATA(rp), MORPH_PHON))
          field = f.c_str();
      int nw = pAMgr->expand_rootword(
          glst, MAX_WORDS, HENTRY_WORD(rp), rp->blen, rp->astr, rp->alen, word,
          nc, field);

      for (int k = 0; k < nw; k++) {
        sc = ngram(n, word, glst[k].word, NGRAM_ANY_MISMATCH + low) +
             leftcommonsubstring(word, glst[k].word);

        if (sc > thresh) {
          if (sc > gscore[lp]) {
            if (guess[lp]) {
              free(guess[lp]);
              if (guessorig[lp]) {
                free(guessorig[lp]);
                guessorig[lp] = NULL;
              }
            }
            gscore[lp] = sc;
            guess[lp] = glst[k].word;
            guessorig[lp] = glst[k].orig;
            lval = sc;
            for (j = 0; j < MAX_GUESS; j++)
              if (gscore[j] < lval) {
                lp = j;
                lval = gscore[j];
              }
          } else {
            free(glst[k].word);
            if (glst[k].orig)
              free(glst[k].orig);
          }
        } else {
          free(glst[k].word);
          if (glst[k].orig)
            free(glst[k].orig);
        }
      }
    }
  }
  free(glst);

  // now we are done generating guesses
  // sort in order of decreasing score

  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);
  if (ph)
    bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // weight suggestions with a similarity index, based on
  // the longest common subsequent algorithm and resort

  int is_swap = 0;
  int re = 0;
  double fact = 1.0;
  if (pAMgr) {
    int maxd = pAMgr->get_maxdiff();
    if (maxd >= 0)
      fact = (10.0 - maxd) / 5.0;
  }

  for (i = 0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      // lowering guess[i]
      std::string gl;
      int len;
      if (utf8) {
        std::vector<w_char> _w;
        len = u8_u16(_w, guess[i]);
        mkallsmall_utf(_w, langnum);
        u16_u8(gl, _w);
      } else {
        gl.assign(guess[i]);
        if (!nonbmp)
          mkallsmall(gl, csconv);
        len = strlen(guess[i]);
      }

      int _lcs = lcslen(word, gl.c_str());

      // same characters with different casing
      if ((n == len) && (n == _lcs)) {
        gscore[i] += 2000;
        break;
      }
      // using 2-gram instead of 3, and other weightening

      re = ngram(2, word, gl, NGRAM_ANY_MISMATCH + low + NGRAM_WEIGHTED) +
           ngram(2, gl, word, NGRAM_ANY_MISMATCH + low + NGRAM_WEIGHTED);

      gscore[i] =
          // length of longest common subsequent minus length difference
          2 * _lcs - abs((int)(n - len)) +
          // weight length of the left common substring
          leftcommonsubstring(word, gl.c_str()) +
          // weight equal character positions
          (!nonbmp && commoncharacterpositions(word, gl.c_str(), &is_swap)
               ? 1
               : 0) +
          // swap character (not neighboring)
          ((is_swap) ? 10 : 0) +
          // ngram
          ngram(4, word, gl, NGRAM_ANY_MISMATCH + low) +
          // weighted ngrams
          re +
          // different limit for dictionaries with PHONE rules
          (ph ? (re < len * fact ? -1000 : 0)
              : (re < (n + len) * fact ? -1000 : 0));
    }
  }

  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);

  // phonetic version
  if (ph)
    for (i = 0; i < MAX_ROOTS; i++) {
      if (rootsphon[i]) {
        // lowering rootphon[i]
        std::string gl;
        int len;
        if (utf8) {
          std::vector<w_char> _w;
          len = u8_u16(_w, rootsphon[i]);
          mkallsmall_utf(_w, langnum);
          u16_u8(gl, _w);
        } else {
          gl.assign(rootsphon[i]);
          if (!nonbmp)
            mkallsmall(gl, csconv);
          len = strlen(rootsphon[i]);
        }

        // heuristic weigthing of ngram scores
        scoresphon[i] += 2 * lcslen(word, gl) - abs((int)(n - len)) +
                         // weight length of the left common substring
                         leftcommonsubstring(word, gl.c_str());
      }
    }

  if (ph)
    bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // copy over
  int oldns = ns;

  int same = 0;
  for (i = 0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      if ((ns < oldns + maxngramsugs) && (ns < maxSug) &&
          (!same || (gscore[i] > 1000))) {
        int unique = 1;
        // leave only excellent suggestions, if exists
        if (gscore[i] > 1000)
          same = 1;
        else if (gscore[i] < -100) {
          same = 1;
          // keep the best ngram suggestions, unless in ONLYMAXDIFF mode
          if (ns > oldns || (pAMgr && pAMgr->get_onlymaxdiff())) {
            free(guess[i]);
            if (guessorig[i])
              free(guessorig[i]);
            continue;
          }
        }
        for (j = 0; j < ns; j++) {
          // don't suggest previous suggestions or a previous suggestion with
          // prefixes or affixes
          if ((!guessorig[i] && strstr(guess[i], wlst[j])) ||
              (guessorig[i] && strstr(guessorig[i], wlst[j])) ||
              // check forbidden words
              !checkword(guess[i], strlen(guess[i]), 0, NULL, NULL)) {
            unique = 0;
            break;
          }
        }
        if (unique) {
          wlst[ns++] = guess[i];
          if (guessorig[i]) {
            free(guess[i]);
            wlst[ns - 1] = guessorig[i];
          }
        } else {
          free(guess[i]);
          if (guessorig[i])
            free(guessorig[i]);
        }
      } else {
        free(guess[i]);
        if (guessorig[i])
          free(guessorig[i]);
      }
    }
  }

  oldns = ns;
  if (ph)
    for (i = 0; i < MAX_ROOTS; i++) {
      if (rootsphon[i]) {
        if ((ns < oldns + MAXPHONSUGS) && (ns < maxSug)) {
          int unique = 1;
          for (j = 0; j < ns; j++) {
            // don't suggest previous suggestions or a previous suggestion with
            // prefixes or affixes
            if (strstr(rootsphon[i], wlst[j]) ||
                // check forbidden words
                !checkword(rootsphon[i], strlen(rootsphon[i]), 0, NULL, NULL)) {
              unique = 0;
              break;
            }
          }
          if (unique) {
            wlst[ns++] = mystrdup(rootsphon[i]);
            if (!wlst[ns - 1])
              return ns - 1;
          }
        }
      }
    }

  if (nonbmp)
    utf8 = 1;
  return ns;
}

// see if a candidate suggestion is spelled correctly
// needs to check both root words and words with affixes

// obsolote MySpell-HU modifications:
// return value 2 and 3 marks compounding with hyphen (-)
// `3' marks roots without suffix
int SuggestMgr::checkword(const char* word,
                          int len,
                          int cpdsuggest,
                          int* timer,
                          clock_t* timelimit) {
  struct hentry* rv = NULL;
  struct hentry* rv2 = NULL;
  int nosuffix = 0;

  // check time limit
  if (timer) {
    (*timer)--;
    if (!(*timer) && timelimit) {
      if ((clock() - *timelimit) > TIMELIMIT)
        return 0;
      *timer = MAXPLUSTIMER;
    }
  }

  if (pAMgr) {
    if (cpdsuggest == 1) {
      if (pAMgr->get_compound()) {
        struct hentry* rwords[100];  // buffer for COMPOUND pattern checking
        rv = pAMgr->compound_check(word, len, 0, 0, 100, 0, NULL, (hentry**)&rwords, 0, 1,
                                   0);  // EXT
        if (rv &&
            (!(rv2 = pAMgr->lookup(word)) || !rv2->astr ||
             !(TESTAFF(rv2->astr, pAMgr->get_forbiddenword(), rv2->alen) ||
               TESTAFF(rv2->astr, pAMgr->get_nosuggest(), rv2->alen))))
          return 3;  // XXX obsolote categorisation + only ICONV needs affix
                     // flag check?
      }
      return 0;
    }

    rv = pAMgr->lookup(word);

    if (rv) {
      if ((rv->astr) &&
          (TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
           TESTAFF(rv->astr, pAMgr->get_nosuggest(), rv->alen)))
        return 0;
      while (rv) {
        if (rv->astr &&
            (TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
             TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen))) {
          rv = rv->next_homonym;
        } else
          break;
      }
    } else
      rv = pAMgr->prefix_check(word, len,
                               0);  // only prefix, and prefix + suffix XXX

    if (rv) {
      nosuffix = 1;
    } else {
      rv = pAMgr->suffix_check(word, len, 0, NULL, NULL, 0,
                               NULL);  // only suffix
    }

    if (!rv && pAMgr->have_contclass()) {
      rv = pAMgr->suffix_check_twosfx(word, len, 0, NULL, FLAG_NULL);
      if (!rv)
        rv = pAMgr->prefix_check_twosfx(word, len, 1, FLAG_NULL);
    }

    // check forbidden words
    if ((rv) && (rv->astr) &&
        (TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
         TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_nosuggest(), rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen)))
      return 0;

    if (rv) {  // XXX obsolote
      if ((pAMgr->get_compoundflag()) &&
          TESTAFF(rv->astr, pAMgr->get_compoundflag(), rv->alen))
        return 2 + nosuffix;
      return 1;
    }
  }
  return 0;
}

int SuggestMgr::check_forbidden(const char* word, int len) {
  struct hentry* rv = NULL;

  if (pAMgr) {
    rv = pAMgr->lookup(word);
    if (rv && rv->astr &&
        (TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen)))
      rv = NULL;
    if (!(pAMgr->prefix_check(word, len, 1)))
      rv = pAMgr->suffix_check(word, len, 0, NULL, NULL, 0,
                               NULL);  // prefix+suffix, suffix
    // check forbidden words
    if ((rv) && (rv->astr) &&
        TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen))
      return 1;
  }
  return 0;
}

char* SuggestMgr::suggest_morph(const char* w) {
  char result[MAXLNLEN];
  char* r = (char*)result;
  char* st;

  struct hentry* rv = NULL;

  *result = '\0';

  if (!pAMgr)
    return NULL;

  std::string w2;
  const char* word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    w2.assign(w);
    if (utf8)
      reverseword_utf(w2);
    else
      reverseword(w2);
    word = w2.c_str();
  }

  rv = pAMgr->lookup(word);

  while (rv) {
    if ((!rv->astr) ||
        !(TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
          TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
          TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen))) {
      if (!HENTRY_FIND(rv, MORPH_STEM)) {
        mystrcat(result, " ", MAXLNLEN);
        mystrcat(result, MORPH_STEM, MAXLNLEN);
        mystrcat(result, word, MAXLNLEN);
      }
      if (HENTRY_DATA(rv)) {
        mystrcat(result, " ", MAXLNLEN);
        mystrcat(result, HENTRY_DATA2(rv), MAXLNLEN);
      }
      mystrcat(result, "\n", MAXLNLEN);
    }
    rv = rv->next_homonym;
  }

  st = pAMgr->affix_check_morph(word, strlen(word));
  if (st) {
    mystrcat(result, st, MAXLNLEN);
    free(st);
  }

  if (pAMgr->get_compound() && (*result == '\0')) {
    struct hentry* rwords[100];  // buffer for COMPOUND pattern checking
    pAMgr->compound_check_morph(word, strlen(word), 0, 0, 100, 0, NULL, (hentry**)&rwords, 0, &r,
                                NULL);
  }

  return (*result) ? mystrdup(line_uniq(result, MSEP_REC)) : NULL;
}

/* affixation */
char* SuggestMgr::suggest_hentry_gen(hentry* rv, const char* pattern) {
  char result[MAXLNLEN];
  *result = '\0';
  int sfxcount = get_sfxcount(pattern);

  if (get_sfxcount(HENTRY_DATA(rv)) > sfxcount)
    return NULL;

  if (HENTRY_DATA(rv)) {
    char* aff = pAMgr->morphgen(HENTRY_WORD(rv), rv->blen, rv->astr, rv->alen,
                                HENTRY_DATA(rv), pattern, 0);
    if (aff) {
      mystrcat(result, aff, MAXLNLEN);
      mystrcat(result, "\n", MAXLNLEN);
      free(aff);
    }
  }

  // check all allomorphs
  char allomorph[MAXLNLEN];
  char* p = NULL;
  if (HENTRY_DATA(rv))
    p = (char*)strstr(HENTRY_DATA2(rv), MORPH_ALLOMORPH);
  while (p) {
    struct hentry* rv2 = NULL;
    p += MORPH_TAG_LEN;
    int plen = fieldlen(p);
    strncpy(allomorph, p, plen);
    allomorph[plen] = '\0';
    rv2 = pAMgr->lookup(allomorph);
    while (rv2) {
      //            if (HENTRY_DATA(rv2) && get_sfxcount(HENTRY_DATA(rv2)) <=
      //            sfxcount) {
      if (HENTRY_DATA(rv2)) {
        char* st = (char*)strstr(HENTRY_DATA2(rv2), MORPH_STEM);
        if (st && (strncmp(st + MORPH_TAG_LEN, HENTRY_WORD(rv),
                           fieldlen(st + MORPH_TAG_LEN)) == 0)) {
          char* aff = pAMgr->morphgen(HENTRY_WORD(rv2), rv2->blen, rv2->astr,
                                      rv2->alen, HENTRY_DATA(rv2), pattern, 0);
          if (aff) {
            mystrcat(result, aff, MAXLNLEN);
            mystrcat(result, "\n", MAXLNLEN);
            free(aff);
          }
        }
      }
      rv2 = rv2->next_homonym;
    }
    p = strstr(p + plen, MORPH_ALLOMORPH);
  }

  return (*result) ? mystrdup(result) : NULL;
}

char* SuggestMgr::suggest_gen(char** desc, int n, const char* pattern) {
  if (n == 0 || !pAMgr)
    return NULL;

  std::string result2;
  std::string newpattern;
  struct hentry* rv = NULL;

  // search affixed forms with and without derivational suffixes
  while (1) {
    for (int k = 0; k < n; k++) {
      std::string result;

      // add compound word parts (except the last one)
      char* s = (char*)desc[k];
      char* part = strstr(s, MORPH_PART);
      if (part) {
        char* nextpart = strstr(part + 1, MORPH_PART);
        while (nextpart) {
          std::string field;
          copy_field(field, part, MORPH_PART);
          result.append(field);
          part = nextpart;
          nextpart = strstr(part + 1, MORPH_PART);
        }
        s = part;
      }

      char** pl;
      std::string tok(s);
      size_t pos = tok.find(" | ");
      while (pos != std::string::npos) {
        tok[pos + 1] = MSEP_ALT;
        pos = tok.find(" | ", pos);
      }
      int pln = line_tok(tok.c_str(), &pl, MSEP_ALT);
      for (int i = 0; i < pln; i++) {
        // remove inflectional and terminal suffixes
        char* is = strstr(pl[i], MORPH_INFL_SFX);
        if (is)
          *is = '\0';
        char* ts = strstr(pl[i], MORPH_TERM_SFX);
        while (ts) {
          *ts = '_';
          ts = strstr(pl[i], MORPH_TERM_SFX);
        }
        char* st = strstr(s, MORPH_STEM);
        if (st) {
          copy_field(tok, st, MORPH_STEM);
          rv = pAMgr->lookup(tok.c_str());
          while (rv) {
            std::string newpat(pl[i]);
            newpat.append(pattern);
            char* sg = suggest_hentry_gen(rv, newpat.c_str());
            if (!sg)
              sg = suggest_hentry_gen(rv, pattern);
            if (sg) {
              char** gen;
              int genl = line_tok(sg, &gen, MSEP_REC);
              free(sg);
              sg = NULL;
              for (int j = 0; j < genl; j++) {
                result2.push_back(MSEP_REC);
                result2.append(result);
                if (strstr(pl[i], MORPH_SURF_PFX)) {
                  std::string field;
                  copy_field(field, pl[i], MORPH_SURF_PFX);
                  result2.append(field);
                }
                result2.append(gen[j]);
              }
              freelist(&gen, genl);
            }
            rv = rv->next_homonym;
          }
        }
      }
      freelist(&pl, pln);
    }

    if (!result2.empty() || !strstr(pattern, MORPH_DERI_SFX))
      break;

    newpattern.assign(pattern);
    mystrrep(newpattern, MORPH_DERI_SFX, MORPH_TERM_SFX);
    pattern = newpattern.c_str();
  }
  return (!result2.empty() ? mystrdup(result2.c_str()) : NULL);
}

// generate an n-gram score comparing s1 and s2
int SuggestMgr::ngram(int n,
                      const std::string& s1,
                      const std::string& s2,
                      int opt) {
  int nscore = 0;
  int ns;
  int l1;
  int l2;
  int test = 0;

  if (utf8) {
    std::vector<w_char> su1;
    std::vector<w_char> su2;
    l1 = u8_u16(su1, s1);
    l2 = u8_u16(su2, s2);
    if ((l2 <= 0) || (l1 == -1))
      return 0;
    // lowering dictionary word
    if (opt & NGRAM_LOWERING)
      mkallsmall_utf(su2, langnum);
    for (int j = 1; j <= n; j++) {
      ns = 0;
      for (int i = 0; i <= (l1 - j); i++) {
        int k = 0;
        for (int l = 0; l <= (l2 - j); l++) {
          for (k = 0; k < j; k++) {
            w_char& c1 = su1[i + k];
            w_char& c2 = su2[l + k];
            if ((c1.l != c2.l) || (c1.h != c2.h))
              break;
          }
          if (k == j) {
            ns++;
            break;
          }
        }
        if (k != j && opt & NGRAM_WEIGHTED) {
          ns--;
          test++;
          if (i == 0 || i == l1 - j)
            ns--;  // side weight
        }
      }
      nscore = nscore + ns;
      if (ns < 2 && !(opt & NGRAM_WEIGHTED))
        break;
    }
  } else {
    l2 = s2.size();
    if (l2 == 0)
      return 0;
    l1 = s1.size();
    std::string t(s2);
    if (opt & NGRAM_LOWERING)
      mkallsmall(t, csconv);
    for (int j = 1; j <= n; j++) {
      ns = 0;
      for (int i = 0; i <= (l1 - j); i++) {
        std::string temp(s1.substr(i, j));
        if (t.find(temp) != std::string::npos) {
          ns++;
        } else if (opt & NGRAM_WEIGHTED) {
          ns--;
          test++;
          if (i == 0 || i == l1 - j)
            ns--;  // side weight
        }
      }
      nscore = nscore + ns;
      if (ns < 2 && !(opt & NGRAM_WEIGHTED))
        break;
    }
  }

  ns = 0;
  if (opt & NGRAM_LONGER_WORSE)
    ns = (l2 - l1) - 2;
  if (opt & NGRAM_ANY_MISMATCH)
    ns = abs(l2 - l1) - 2;
  ns = (nscore - ((ns > 0) ? ns : 0));
  return ns;
}

// length of the left common substring of s1 and (decapitalised) s2
int SuggestMgr::leftcommonsubstring(const char* s1, const char* s2) {
  if (utf8) {
    std::vector<w_char> su1;
    std::vector<w_char> su2;
    int l1 = u8_u16(su1, s1);
    int l2 = u8_u16(su2, s2);
    // decapitalize dictionary word
    if (complexprefixes) {
      if (su1[l1 - 1] == su2[l2 - 1])
        return 1;
    } else {
      unsigned short idx = su2.empty() ? 0 : (su2[0].h << 8) + su2[0].l;
      unsigned short otheridx = su1.empty() ? 0 : (su1[0].h << 8) + su1[0].l;
      if (otheridx != idx && (otheridx != unicodetolower(idx, langnum)))
        return 0;
      int i;
      for (i = 1; (i < l1) && (i < l2) && (su1[i].l == su2[i].l) &&
                  (su1[i].h == su2[i].h);
           i++)
        ;
      return i;
    }
  } else {
    if (complexprefixes) {
      int l1 = strlen(s1);
      int l2 = strlen(s2);
      if (l1 <= l2 && s2[l1 - 1] == s2[l2 - 1])
        return 1;
    } else if (csconv) {
      const char* olds = s1;
      // decapitalise dictionary word
      if ((*s1 != *s2) && (*s1 != csconv[((unsigned char)*s2)].clower))
        return 0;
      do {
        s1++;
        s2++;
      } while ((*s1 == *s2) && (*s1 != '\0'));
      return (int)(s1 - olds);
    }
  }
  return 0;
}

int SuggestMgr::commoncharacterpositions(const char* s1,
                                         const char* s2,
                                         int* is_swap) {
  int num = 0;
  int diff = 0;
  int diffpos[2];
  *is_swap = 0;
  if (utf8) {
    std::vector<w_char> su1;
    std::vector<w_char> su2;
    int l1 = u8_u16(su1, s1);
    int l2 = u8_u16(su2, s2);

    if (l1 <= 0 || l2 <= 0)
      return 0;

    // decapitalize dictionary word
    if (complexprefixes) {
      su2[l2 - 1] = lower_utf(su2[l2 - 1], langnum);
    } else {
      su2[0] = lower_utf(su2[0], langnum);
    }
    for (int i = 0; (i < l1) && (i < l2); i++) {
      if (su1[i] == su2[i]) {
        num++;
      } else {
        if (diff < 2)
          diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (l1 == l2) &&
        (su1[diffpos[0]] == su2[diffpos[1]]) &&
        (su1[diffpos[1]] == su2[diffpos[0]]))
      *is_swap = 1;
  } else {
    size_t i;
    std::string t(s2);
    // decapitalize dictionary word
    if (complexprefixes) {
      size_t l2 = t.size();
      t[l2 - 1] = csconv[(unsigned char)t[l2 - 1]].clower;
    } else {
      mkallsmall(t, csconv);
    }
    for (i = 0; (*(s1 + i) != 0) && i < t.size(); i++) {
      if (*(s1 + i) == t[i]) {
        num++;
      } else {
        if (diff < 2)
          diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (*(s1 + i) == 0) && i == t.size() &&
        (*(s1 + diffpos[0]) == t[diffpos[1]]) &&
        (*(s1 + diffpos[1]) == t[diffpos[0]]))
      *is_swap = 1;
  }
  return num;
}

int SuggestMgr::mystrlen(const char* word) {
  if (utf8) {
    std::vector<w_char> w;
    return u8_u16(w, word);
  } else
    return strlen(word);
}

// sort in decreasing order of score
void SuggestMgr::bubblesort(char** rword, char** rword2, int* rsc, int n) {
  int m = 1;
  while (m < n) {
    int j = m;
    while (j > 0) {
      if (rsc[j - 1] < rsc[j]) {
        int sctmp = rsc[j - 1];
        char* wdtmp = rword[j - 1];
        rsc[j - 1] = rsc[j];
        rword[j - 1] = rword[j];
        rsc[j] = sctmp;
        rword[j] = wdtmp;
        if (rword2) {
          wdtmp = rword2[j - 1];
          rword2[j - 1] = rword2[j];
          rword2[j] = wdtmp;
        }
        j--;
      } else
        break;
    }
    m++;
  }
  return;
}

// longest common subsequence
void SuggestMgr::lcs(const char* s,
                     const char* s2,
                     int* l1,
                     int* l2,
                     char** result) {
  int n, m;
  std::vector<w_char> su;
  std::vector<w_char> su2;
  char* b;
  char* c;
  int i;
  int j;
  if (utf8) {
    m = u8_u16(su, s);
    n = u8_u16(su2, s2);
  } else {
    m = strlen(s);
    n = strlen(s2);
  }
  c = (char*)malloc((m + 1) * (n + 1));
  b = (char*)malloc((m + 1) * (n + 1));
  if (!c || !b) {
    if (c)
      free(c);
    if (b)
      free(b);
    *result = NULL;
    return;
  }
  for (i = 1; i <= m; i++)
    c[i * (n + 1)] = 0;
  for (j = 0; j <= n; j++)
    c[j] = 0;
  for (i = 1; i <= m; i++) {
    for (j = 1; j <= n; j++) {
      if (((utf8) && (su[i - 1] == su2[j - 1])) ||
          ((!utf8) && (s[i - 1] == s2[j - 1]))) {
        c[i * (n + 1) + j] = c[(i - 1) * (n + 1) + j - 1] + 1;
        b[i * (n + 1) + j] = LCS_UPLEFT;
      } else if (c[(i - 1) * (n + 1) + j] >= c[i * (n + 1) + j - 1]) {
        c[i * (n + 1) + j] = c[(i - 1) * (n + 1) + j];
        b[i * (n + 1) + j] = LCS_UP;
      } else {
        c[i * (n + 1) + j] = c[i * (n + 1) + j - 1];
        b[i * (n + 1) + j] = LCS_LEFT;
      }
    }
  }
  *result = b;
  free(c);
  *l1 = m;
  *l2 = n;
}

int SuggestMgr::lcslen(const char* s, const char* s2) {
  int m;
  int n;
  int i;
  int j;
  char* result;
  int len = 0;
  lcs(s, s2, &m, &n, &result);
  if (!result)
    return 0;
  i = m;
  j = n;
  while ((i != 0) && (j != 0)) {
    if (result[i * (n + 1) + j] == LCS_UPLEFT) {
      len++;
      i--;
      j--;
    } else if (result[i * (n + 1) + j] == LCS_UP) {
      i--;
    } else
      j--;
  }
  free(result);
  return len;
}

int SuggestMgr::lcslen(const std::string& s, const std::string& s2) {
  return lcslen(s.c_str(), s2.c_str());
}
