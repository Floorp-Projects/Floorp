/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Kevin Hendricks (kevin.hendricks@sympatico.ca)
 *                 David Einstein (deinst@world.std.com)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Davide Prina
 *                 Giuseppe Modugno
 *                 Gianluca Turconi
 *                 Simon Brouwer
 *                 Noll Janos
 *                 Biro Arpad
 *                 Goldman Eleonora
 *                 Sarlos Tamas
 *                 Bencsath Boldizsar
 *                 Halacsy Peter
 *                 Dvornik Laszlo
 *                 Gefferth Andras
 *                 Nagy Viktor
 *                 Varga Daniel
 *                 Chris Halls
 *                 Rene Engelhard
 *                 Bram Moolenaar
 *                 Dafydd Jones
 *                 Harri Pitkanen
 *                 Andras Timar
 *                 Tor Lillqvist
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
 ******* END LICENSE BLOCK *******/

#ifndef MOZILLA_CLIENT
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#else
#include <stdlib.h> 
#include <string.h>
#include <stdio.h> 
#include <ctype.h>
#endif

#include "suggestmgr.hxx"

#ifndef MOZILLA_CLIENT
#ifndef W32
using namespace std;
#endif
#endif

const w_char W_VLINE = { '\0', '|' };

SuggestMgr::SuggestMgr(const char * tryme, int maxn, 
                       AffixMgr * aptr)
{

  // register affix manager and check in string of chars to 
  // try when building candidate suggestions
  pAMgr = aptr;

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

  if (pAMgr) {
        char * enc = pAMgr->get_encoding();
        csconv = get_current_cs(enc);
        free(enc);
        langnum = pAMgr->get_langnum();
        ckey = pAMgr->get_key_string();
        nosplitsugs = pAMgr->get_nosplitsugs();
        if (pAMgr->get_maxngramsugs() >= 0) maxngramsugs = pAMgr->get_maxngramsugs();
        utf8 = pAMgr->get_utf8();
        complexprefixes = pAMgr->get_complexprefixes();
  }

  if (ckey) {  
    if (utf8) {
        w_char t[MAXSWL];    
        ckeyl = u8_u16(t, MAXSWL, ckey);
        ckey_utf = (w_char *) malloc(ckeyl * sizeof(w_char));
        if (ckey_utf) memcpy(ckey_utf, t, ckeyl * sizeof(w_char));
    } else {
        ckeyl = strlen(ckey);
    }
  }
  
  if (tryme) {  
    if (utf8) {
        w_char t[MAXSWL];    
        ctryl = u8_u16(t, MAXSWL, tryme);
        ctry_utf = (w_char *) malloc(ctryl * sizeof(w_char));
        if (ctry_utf) memcpy(ctry_utf, t, ctryl * sizeof(w_char));
    } else {
        ctry = mystrdup(tryme);
        ctryl = strlen(ctry);
    }
  }
}


SuggestMgr::~SuggestMgr()
{
  pAMgr = NULL;
  if (ckey) free(ckey);
  ckey = NULL;
  if (ckey_utf) free(ckey_utf);
  ckey_utf = NULL;
  ckeyl = 0;
  if (ctry) free(ctry);
  ctry = NULL;
  if (ctry_utf) free(ctry_utf);
  ctry_utf = NULL;
  ctryl = 0;
  maxSug = 0;
}

int SuggestMgr::testsug(char** wlst, const char * candidate, int wl, int ns, int cpdsuggest,
   int * timer, clock_t * timelimit) {
      int cwrd = 1;
      if (ns == maxSug) return maxSug;
      for (int k=0; k < ns; k++) {
        if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
      }
      if ((cwrd) && checkword(candidate, wl, cpdsuggest, timer, timelimit)) {
        wlst[ns] = mystrdup(candidate);
        if (wlst[ns] == NULL) {
            for (int j=0; j<ns; j++) free(wlst[j]);
            return -1;
        }
        ns++;
      } 
      return ns;
}

// generate suggestions for a mispelled word
//    pass in address of array of char * pointers
// onlycompoundsug: probably bad suggestions (need for ngram sugs, too)

int SuggestMgr::suggest(char*** slst, const char * w, int nsug,
    int * onlycompoundsug)
{
  int nocompoundtwowords = 0;
  char ** wlst;    
  w_char word_utf[MAXSWL];
  int wl = 0;

  char w2[MAXWORDUTF8LEN];
  const char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }
    
    if (*slst) {
        wlst = *slst;
    } else {
        wlst = (char **) malloc(maxSug * sizeof(char *));
        if (wlst == NULL) return -1;
        for (int i = 0; i < maxSug; i++) {
            wlst[i] = NULL;
        }
    }
    
    if (utf8) {
        wl = u8_u16(word_utf, MAXSWL, word);
    }

    for (int cpdsuggest=0; (cpdsuggest<2) && (nocompoundtwowords==0); cpdsuggest++) {

    // suggestions for an uppercase word (html -> HTML)
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? capchars_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    capchars(wlst, word, nsug, cpdsuggest);
    }

    // perhaps we made a typical fault of spelling
    if ((nsug < maxSug) && (nsug > -1))
    nsug = replchars(wlst, word, nsug, cpdsuggest);

    // perhaps we made chose the wrong char from a related set
    if ((nsug < maxSug) && (nsug > -1)) {
      nsug = mapchars(wlst, word, nsug, cpdsuggest);
    }

    // did we swap the order of chars by mistake
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? swapchar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    swapchar(wlst, word, nsug, cpdsuggest);
    }

    // did we swap the order of non adjacent chars by mistake
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? longswapchar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    longswapchar(wlst, word, nsug, cpdsuggest);
    }

    // did we just hit the wrong key in place of a good char (case and keyboard)
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? badcharkey_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    badcharkey(wlst, word, nsug, cpdsuggest);
    }

    // did we add a char that should not be there
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? extrachar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    extrachar(wlst, word, nsug, cpdsuggest);
    }

    // only suggest compound words when no other suggestion
    if ((cpdsuggest == 0) && (nsug > 0)) nocompoundtwowords=1;

    // did we forgot a char
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? forgotchar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    forgotchar(wlst, word, nsug, cpdsuggest);
    }

    // did we move a char
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? movechar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    movechar(wlst, word, nsug, cpdsuggest);
    }

    // did we just hit the wrong key in place of a good char
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? badchar_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    badchar(wlst, word, nsug, cpdsuggest);
    }

    // did we double two characters
    if ((nsug < maxSug) && (nsug > -1)) {
        nsug = (utf8) ? doubletwochars_utf(wlst, word_utf, wl, nsug, cpdsuggest) :
                    doubletwochars(wlst, word, nsug, cpdsuggest);
    }

    // perhaps we forgot to hit space and two words ran together
    if ((!nosplitsugs) && (nsug < maxSug) && (nsug > -1)) {
                nsug = twowords(wlst, word, nsug, cpdsuggest);
        }

    } // repeating ``for'' statement compounding support

    if (nsug < 0) {
     // we ran out of memory - we should free up as much as possible
       for (int i = 0; i < maxSug; i++)
         if (wlst[i] != NULL) free(wlst[i]);
       free(wlst);
       wlst = NULL;
    }
    
    if (!nocompoundtwowords && (nsug > 0) && onlycompoundsug) *onlycompoundsug = 1;

    *slst = wlst;
    return nsug;
}

// generate suggestions for a word with typical mistake
//    pass in address of array of char * pointers
#ifdef HUNSPELL_EXPERIMENTAL
int SuggestMgr::suggest_auto(char*** slst, const char * w, int nsug)
{
    int nocompoundtwowords = 0;
    char ** wlst;

  char w2[MAXWORDUTF8LEN];
  const char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }

    if (*slst) {
        wlst = *slst;
    } else {
        wlst = (char **) malloc(maxSug * sizeof(char *));
        if (wlst == NULL) return -1;
    }

    for (int cpdsuggest=0; (cpdsuggest<2) && (nocompoundtwowords==0); cpdsuggest++) {

    // perhaps we made a typical fault of spelling
    if ((nsug < maxSug) && (nsug > -1))
    nsug = replchars(wlst, word, nsug, cpdsuggest);

    // perhaps we made chose the wrong char from a related set
    if ((nsug < maxSug) && (nsug > -1))
      nsug = mapchars(wlst, word, nsug, cpdsuggest);

    if ((cpdsuggest==0) && (nsug>0)) nocompoundtwowords=1; else *

    // perhaps we forgot to hit space and two words ran together

    if ((nsug < maxSug) && (nsug > -1) && check_forbidden(word, strlen(word))) {
                nsug = twowords(wlst, word, nsug, cpdsuggest);
        }
    
    } // repeating ``for'' statement compounding support

    if (nsug < 0) {
       for (int i=0;i<maxSug; i++)
         if (wlst[i] != NULL) free(wlst[i]);
       free(wlst);
       return -1;
    }

    *slst = wlst;
    return nsug;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE

// suggestions for an uppercase word (html -> HTML)
int SuggestMgr::capchars_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
  char candidate[MAXSWUTF8L];
  w_char candidate_utf[MAXSWL];
  memcpy(candidate_utf, word, wl * sizeof(w_char));
  mkallcap_utf(candidate_utf, wl, langnum);
  u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
  return testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
}

// suggestions for an uppercase word (html -> HTML)
int SuggestMgr::capchars(char** wlst, const char * word, int ns, int cpdsuggest)
{
  char candidate[MAXSWUTF8L];
  strcpy(candidate, word);
  mkallcap(candidate, csconv);
  return testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
}

// suggestions for when chose the wrong char out of a related set
int SuggestMgr::mapchars(char** wlst, const char * word, int ns, int cpdsuggest)
{
  clock_t timelimit;
  int timer;
  
  int wl = strlen(word);
  if (wl < 2 || ! pAMgr) return ns;

  int nummap = pAMgr->get_nummap();
  struct mapentry* maptable = pAMgr->get_maptable();
  if (maptable==NULL) return ns;

  timelimit = clock();
  timer = MINTIMER;
  if (utf8) {
    w_char w[MAXSWL];
    int len = u8_u16(w, MAXSWL, word);
    ns = map_related_utf(w, len, 0, cpdsuggest, wlst, ns, maptable, nummap, &timer, &timelimit);
  } else ns = map_related(word, 0, wlst, cpdsuggest, ns, maptable, nummap, &timer, &timelimit);
  return ns;
}

int SuggestMgr::map_related(const char * word, int i, char** wlst, 
    int cpdsuggest,  int ns,
    const mapentry* maptable, int nummap, int * timer, clock_t * timelimit)
{
  char c = *(word + i);  
  if (c == 0) {
      int cwrd = 1;
      int wl = strlen(word);
      for (int m=0; m < ns; m++)
          if (strcmp(word,wlst[m]) == 0) cwrd = 0;
      if ((cwrd) && checkword(word, wl, cpdsuggest, timer, timelimit)) {
          if (ns < maxSug) {
              wlst[ns] = mystrdup(word);
              if (wlst[ns] == NULL) return -1;
              ns++;
          }
      }
      return ns;
  } 
  int in_map = 0;
  for (int j = 0; j < nummap; j++) {
    if (strchr(maptable[j].set,c) != 0) {
      in_map = 1;
      char * newword = mystrdup(word);
      for (int k = 0; k < maptable[j].len; k++) {
        *(newword + i) = *(maptable[j].set + k);
        ns = map_related(newword, (i+1), wlst, cpdsuggest,
           ns, maptable, nummap, timer, timelimit);
        if (!(*timer)) return ns;
      }
      free(newword);
    }
  }
  if (!in_map) {
     i++;
     ns = map_related(word, i, wlst, cpdsuggest,
        ns, maptable, nummap, timer, timelimit);
  }
  return ns;
}

int SuggestMgr::map_related_utf(w_char * word, int len, int i, int cpdsuggest,
    char** wlst, int ns, const mapentry* maptable, int nummap,
    int * timer, clock_t * timelimit) 
{
  if (i == len) {
      int cwrd = 1;
      int wl;
      char s[MAXSWUTF8L];
      u16_u8(s, MAXSWUTF8L, word, len);
      wl = strlen(s);
      for (int m=0; m < ns; m++)
          if (strcmp(s,wlst[m]) == 0) cwrd = 0;
      if ((cwrd) && checkword(s, wl, cpdsuggest, timer, timelimit)) {
          if (ns < maxSug) {
              wlst[ns] = mystrdup(s);
              if (wlst[ns] == NULL) return -1;
              ns++;
          }
      }
      return ns;
  } 
  int in_map = 0;
  unsigned short c = *((unsigned short *) word + i);
  for (int j = 0; j < nummap; j++) {
    if (flag_bsearch((unsigned short *) maptable[j].set_utf16, c, maptable[j].len)) {
      in_map = 1;
      for (int k = 0; k < maptable[j].len; k++) {
        *(word + i) = *(maptable[j].set_utf16 + k);
        ns = map_related_utf(word, len, i + 1, cpdsuggest,
           wlst, ns, maptable, nummap, timer, timelimit);
        if (!(*timer)) return ns;
      }
      *((unsigned short *) word + i) = c;
    }
  }
  if (!in_map) {
     i++;
     ns = map_related_utf(word, len, i, cpdsuggest,
          wlst, ns, maptable, nummap, timer, timelimit);
  }
  return ns;
}



// suggestions for a typical fault of spelling, that
// differs with more, than 1 letter from the right form.
int SuggestMgr::replchars(char** wlst, const char * word, int ns, int cpdsuggest)
{
  char candidate[MAXSWUTF8L];
  const char * r;
  int lenr, lenp;
  int wl = strlen(word);
  if (wl < 2 || ! pAMgr) return ns;
  int numrep = pAMgr->get_numrep();
  struct replentry* reptable = pAMgr->get_reptable();
  if (reptable==NULL) return ns;
  for (int i=0; i < numrep; i++ ) {
      r = word;
      lenr = strlen(reptable[i].pattern2);
      lenp = strlen(reptable[i].pattern);
      // search every occurence of the pattern in the word
      while ((r=strstr(r, reptable[i].pattern)) != NULL) {
          strcpy(candidate, word);
          if (r-word + lenr + strlen(r+lenp) >= MAXSWUTF8L) break;
          strcpy(candidate+(r-word),reptable[i].pattern2);
          strcpy(candidate+(r-word)+lenr, r+lenp);
          ns = testsug(wlst, candidate, wl-lenp+lenr, ns, cpdsuggest, NULL, NULL);
          if (ns == -1) return -1;
          // check REP suggestions with space
          char * sp = strchr(candidate, ' ');
          if (sp) {
            *sp = '\0';
            if (checkword(candidate, strlen(candidate), 0, NULL, NULL)) {
              int oldns = ns;
              *sp = ' ';
              ns = testsug(wlst, sp + 1, strlen(sp + 1), ns, cpdsuggest, NULL, NULL);
              if (ns == -1) return -1;
              if (oldns < ns) {
                free(wlst[ns - 1]);
                wlst[ns - 1] = mystrdup(candidate);
              }
            }            
            *sp = ' ';
          }
          r++; // search for the next letter
      }
   }
   return ns;
}

// perhaps we doubled two characters (pattern aba -> ababa, for example vacation -> vacacation)
int SuggestMgr::doubletwochars(char** wlst, const char * word, int ns, int cpdsuggest)
{
  char candidate[MAXSWUTF8L];
  int state=0;
  int wl = strlen(word);
  if (wl < 5 || ! pAMgr) return ns;
  for (int i=2; i < wl; i++ ) {
      if (word[i]==word[i-2]) {
          state++;
          if (state==3) {
            strcpy(candidate,word);
            strcpy(candidate+i-1,word+i+1);
            ns = testsug(wlst, candidate, wl-2, ns, cpdsuggest, NULL, NULL);
            if (ns == -1) return -1;
            state=0;
          }
      } else {
            state=0;
      }
  }
  return ns;
}

// perhaps we doubled two characters (pattern aba -> ababa, for example vacation -> vacacation)
int SuggestMgr::doubletwochars_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
  w_char        candidate_utf[MAXSWL];
  char          candidate[MAXSWUTF8L];
  int state=0;
  if (wl < 5 || ! pAMgr) return ns;
  for (int i=2; i < wl; i++) {
      if (w_char_eq(word[i], word[i-2]))  {
          state++;
          if (state==3) {
            memcpy(candidate_utf, word, (i - 1) * sizeof(w_char));
            memcpy(candidate_utf+i-1, word+i+1, (wl-i-1) * sizeof(w_char));
            u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl-2);
            ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
            if (ns == -1) return -1;
            state=0;
          }
      } else {
            state=0;
      }
  }
  return ns;
}

// error is wrong char in place of correct one (case and keyboard related version)
int SuggestMgr::badcharkey(char ** wlst, const char * word, int ns, int cpdsuggest)
{
  char  tmpc;
  char  candidate[MAXSWUTF8L];
  int wl = strlen(word);
  strcpy(candidate, word);
  // swap out each char one by one and try uppercase and neighbor
  // keyboard chars in its place to see if that makes a good word

  for (int i=0; i < wl; i++) {
    tmpc = candidate[i];
    // check with uppercase letters
    candidate[i] = csconv[((unsigned char)tmpc)].cupper;
    if (tmpc != candidate[i]) {
       ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
       if (ns == -1) return -1;
       candidate[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey) continue;
    char * loc = strchr(ckey, tmpc);
    while (loc) {
       if ((loc > ckey) && (*(loc - 1) != '|')) {
          candidate[i] = *(loc - 1);
          ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
          if (ns == -1) return -1;
       }
       if ((*(loc + 1) != '|') && (*(loc + 1) != '\0')) {
          candidate[i] = *(loc + 1);
          ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
          if (ns == -1) return -1;
       }
       loc = strchr(loc + 1, tmpc);
    }
    candidate[i] = tmpc;    
  }
  return ns;
}

// error is wrong char in place of correct one (case and keyboard related version)
int SuggestMgr::badcharkey_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
  w_char        tmpc;
  w_char        candidate_utf[MAXSWL];
  char          candidate[MAXSWUTF8L];
  memcpy(candidate_utf, word, wl * sizeof(w_char));
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i=0; i < wl; i++) {
    tmpc = candidate_utf[i];
    // check with uppercase letters
    mkallcap_utf(candidate_utf + i, 1, langnum);
    if (!w_char_eq(tmpc, candidate_utf[i])) {
       u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
       ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
       if (ns == -1) return -1;
       candidate_utf[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey) continue;
    w_char * loc = ckey_utf;
    while ((loc < (ckey_utf + ckeyl)) && !w_char_eq(*loc, tmpc)) loc++;
    while (loc < (ckey_utf + ckeyl)) {
       if ((loc > ckey_utf) && !w_char_eq(*(loc - 1), W_VLINE)) {
          candidate_utf[i] = *(loc - 1);
          u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
          ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
          if (ns == -1) return -1;
       }
       if (((loc + 1) < (ckey_utf + ckeyl)) && !w_char_eq(*(loc + 1), W_VLINE)) {
          candidate_utf[i] = *(loc + 1);
          u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
          ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
          if (ns == -1) return -1;
       }
       do { loc++; } while ((loc < (ckey_utf + ckeyl)) && !w_char_eq(*loc, tmpc));
    }
    candidate_utf[i] = tmpc;    
  }
  return ns;
}

// error is wrong char in place of correct one
int SuggestMgr::badchar(char ** wlst, const char * word, int ns, int cpdsuggest)
{
  char  tmpc;
  char  candidate[MAXSWUTF8L];
  clock_t timelimit = clock();
  int timer = MINTIMER;
  int wl = strlen(word);
  strcpy(candidate, word);
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i=0; i < wl; i++) {
    tmpc = candidate[i];
    for (int j=0; j < ctryl; j++) {
       if (ctry[j] == tmpc) continue;
       candidate[i] = ctry[j];
       ns = testsug(wlst, candidate, wl, ns, cpdsuggest, &timer, &timelimit);
       if (ns == -1) return -1;
       if (!timer) return ns;
       candidate[i] = tmpc;
    }
  }
  return ns;
}

// error is wrong char in place of correct one
int SuggestMgr::badchar_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
  w_char        tmpc;
  w_char        candidate_utf[MAXSWL];
  char          candidate[MAXSWUTF8L];
  clock_t timelimit = clock();
  int timer = MINTIMER;  
  memcpy(candidate_utf, word, wl * sizeof(w_char));
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i=0; i < wl; i++) {
    tmpc = candidate_utf[i];
    for (int j=0; j < ctryl; j++) {
       if (w_char_eq(tmpc, ctry_utf[j])) continue;
       candidate_utf[i] = ctry_utf[j];
       u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
       ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, &timer, &timelimit);
       if (ns == -1) return -1;
       if (!timer) return ns;
       candidate_utf[i] = tmpc;
    }
  }
  return ns;
}

// error is word has an extra letter it does not need 
int SuggestMgr::extrachar_utf(char** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
   char candidate[MAXSWUTF8L];
   w_char candidate_utf[MAXSWL];
   const w_char * p;
   w_char * r;
   if (wl < 2) return ns;
   // try omitting one char of word at a time
   memcpy(candidate_utf, word + 1, (wl - 1) * sizeof(w_char));
   for (p = word, r = candidate_utf;  p < word + wl;  ) {
       u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl - 1);       
       ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
       if (ns == -1) return -1;
       *r++ = *p++;
   }
   return ns;
}

// error is word has an extra letter it does not need 
int SuggestMgr::extrachar(char** wlst, const char * word, int ns, int cpdsuggest)
{
   char    candidate[MAXSWUTF8L];
   const char *  p;
   char *  r;
   int wl = strlen(word);
   if (wl < 2) return ns;
   // try omitting one char of word at a time
   strcpy (candidate, word + 1);
   for (p = word, r = candidate;  *p != 0;  ) {
      ns = testsug(wlst, candidate, wl-1, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
      *r++ = *p++;
   }
   return ns;
}


// error is missing a letter it needs
int SuggestMgr::forgotchar(char ** wlst, const char * word, int ns, int cpdsuggest)
{
   char candidate[MAXSWUTF8L];
   const char * p;
   char *       q;
   clock_t timelimit = clock();
   int timer = MINTIMER;
   int wl = strlen(word);
   // try inserting a tryme character before every letter
   strcpy(candidate + 1, word);
   for (p = word, q = candidate;  *p != 0;  )  {
      for (int i = 0;  i < ctryl;  i++) {
         *q = ctry[i];
         ns = testsug(wlst, candidate, wl+1, ns, cpdsuggest, &timer, &timelimit);
         if (ns == -1) return -1;
         if (!timer) return ns;
      }
      *q++ = *p++;
   }
   // now try adding one to end */
   for (int i = 0;  i < ctryl;  i++) {
      *q = ctry[i];
      ns = testsug(wlst, candidate, wl+1, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
   }
   return ns;
}

// error is missing a letter it needs
int SuggestMgr::forgotchar_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
   w_char  candidate_utf[MAXSWL];
   char    candidate[MAXSWUTF8L];
   const w_char * p;
   w_char * q;
   int cwrd;
   clock_t timelimit = clock();
   int timer = MINTIMER;
   // try inserting a tryme character before every letter
   memcpy (candidate_utf + 1, word, wl * sizeof(w_char));
   for (p = word, q = candidate_utf;  p < (word + wl); )  {
      for (int i = 0;  i < ctryl;  i++) {
         *q = ctry_utf[i];
         cwrd = 1;
         u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl + 1);
         ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, &timer, &timelimit);
         if (ns == -1) return -1;
         if (!timer) return ns;
       }
      *q++ = *p++;
   }
   // now try adding one to end */
   for (int i = 0;  i < ctryl;  i++) {
      *q = ctry_utf[i];
      cwrd = 1;
      u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl + 1);
      ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
   }
   return ns;
}


/* error is should have been two words */
int SuggestMgr::twowords(char ** wlst, const char * word, int ns, int cpdsuggest)
{
    char candidate[MAXSWUTF8L];
    char * p;
    int c1, c2;
    int forbidden = 0;
    int cwrd;

    int wl=strlen(word);
    if (wl < 3) return ns;
    
    if (langnum == LANG_hu) forbidden = check_forbidden(word, wl);

    strcpy(candidate + 1, word);
    // split the string into two pieces after every char
    // if both pieces are good words make them a suggestion
    for (p = candidate + 1;  p[1] != '\0';  p++) {
       p[-1] = *p;
       // go to end of the UTF-8 character
       while (utf8 && ((p[1] & 0xc0) == 0x80)) {
         *p = p[1];
         p++;
       }
       if (utf8 && p[1] == '\0') break; // last UTF-8 character
       *p = '\0';
       c1 = checkword(candidate,strlen(candidate), cpdsuggest, NULL, NULL);
       if (c1) {
         c2 = checkword((p+1),strlen(p+1), cpdsuggest, NULL, NULL);
         if (c2) {
            *p = ' ';

            // spec. Hungarian code (need a better compound word support)
            if ((langnum == LANG_hu) && !forbidden &&
                // if 3 repeating letter, use - instead of space
                (((p[-1] == p[1]) && (((p>candidate+1) && (p[-1] == p[-2])) || (p[-1] == p[2]))) ||
                // or multiple compounding, with more, than 6 syllables
                ((c1 == 3) && (c2 >= 2)))) *p = '-';

            cwrd = 1;
            for (int k=0; k < ns; k++)
                if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
            if (ns < maxSug) {
                if (cwrd) {
                    wlst[ns] = mystrdup(candidate);
                    if (wlst[ns] == NULL) return -1;
                    ns++;
                }
            } else return ns;
         }
       }
    }
    return ns;
}


// error is adjacent letter were swapped
int SuggestMgr::swapchar(char ** wlst, const char * word, int ns, int cpdsuggest)
{
   char candidate[MAXSWUTF8L];
   char * p;
   char tmpc;
   int wl=strlen(word);
   // try swapping adjacent chars one by one
   strcpy(candidate, word);
   for (p = candidate;  p[1] != 0;  p++) {
      tmpc = *p;
      *p = p[1];
      p[1] = tmpc;
      ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
      p[1] = *p;
      *p = tmpc;
   }
   // try double swaps for short words
   // ahev -> have, owudl -> would
   if (wl == 4 || wl == 5) {
     candidate[0] = word[1];
     candidate[1] = word[0];
     candidate[2] = word[2];
     candidate[wl - 2] = word[wl - 1];
     candidate[wl - 1] = word[wl - 2];
     ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
     if (ns == -1) return -1;
     if (wl == 5) {
        candidate[0] = word[0];
        candidate[1] = word[2];
        candidate[2] = word[1];
        ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
        if (ns == -1) return -1;
     }
   }
   return ns;
}

// error is adjacent letter were swapped
int SuggestMgr::swapchar_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
   w_char candidate_utf[MAXSWL];
   char   candidate[MAXSWUTF8L];
   w_char * p;
   w_char tmpc;
   int len = 0;
   // try swapping adjacent chars one by one
   memcpy (candidate_utf, word, wl * sizeof(w_char));
   for (p = candidate_utf;  p < (candidate_utf + wl - 1);  p++) {
      tmpc = *p;
      *p = p[1];
      p[1] = tmpc;
      u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
      if (len == 0) len = strlen(candidate);
      ns = testsug(wlst, candidate, len, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
      p[1] = *p;
      *p = tmpc;
   }
   // try double swaps for short words
   // ahev -> have, owudl -> would, suodn -> sound
   if (wl == 4 || wl == 5) {
     candidate_utf[0] = word[1];
     candidate_utf[1] = word[0];
     candidate_utf[2] = word[2];
     candidate_utf[wl - 2] = word[wl - 1];
     candidate_utf[wl - 1] = word[wl - 2];
     u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
     ns = testsug(wlst, candidate, len, ns, cpdsuggest, NULL, NULL);
     if (ns == -1) return -1;
     if (wl == 5) {
        candidate_utf[0] = word[0];
        candidate_utf[1] = word[2];
        candidate_utf[2] = word[1];
        u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
	ns = testsug(wlst, candidate, len, ns, cpdsuggest, NULL, NULL);
        if (ns == -1) return -1;
     }
   }
   return ns;
}

// error is not adjacent letter were swapped
int SuggestMgr::longswapchar(char ** wlst, const char * word, int ns, int cpdsuggest)
{
   char candidate[MAXSWUTF8L];
   char * p;
   char * q;
   char tmpc;
   int wl=strlen(word);
   // try swapping not adjacent chars one by one
   strcpy(candidate, word);
   for (p = candidate;  *p != 0;  p++) {
    for (q = candidate;  *q != 0;  q++) {
     if (abs(p-q) > 1) {
      tmpc = *p;
      *p = *q;
      *q = tmpc;
      ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
      *q = *p;
      *p = tmpc;
     }
    }
   }
   return ns;
}


// error is adjacent letter were swapped
int SuggestMgr::longswapchar_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
   w_char candidate_utf[MAXSWL];
   char   candidate[MAXSWUTF8L];
   w_char * p;
   w_char * q;
   w_char tmpc;
   // try swapping not adjacent chars
   memcpy (candidate_utf, word, wl * sizeof(w_char));
   for (p = candidate_utf;  p < (candidate_utf + wl);  p++) {
     for (q = candidate_utf;  q < (candidate_utf + wl);  q++) {
       if (abs(p-q) > 1) {
         tmpc = *p;
         *p = *q;
         *q = tmpc;
         u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
         ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
         if (ns == -1) return -1;
         *q = *p;
         *p = tmpc;
       }
     }
   }
   return ns;
}

// error is a letter was moved
int SuggestMgr::movechar(char ** wlst, const char * word, int ns, int cpdsuggest)
{
   char candidate[MAXSWUTF8L];
   char * p;
   char * q;
   char tmpc;

   int wl=strlen(word);
   // try moving a char
   strcpy(candidate, word);
   for (p = candidate;  *p != 0;  p++) {
     for (q = p + 1;  (*q != 0) && ((q - p) < 10);  q++) {
      tmpc = *(q-1);
      *(q-1) = *q;
      *q = tmpc;
      if ((q-p) < 2) continue; // omit swap char
      ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
    }
    strcpy(candidate, word);
   }
   for (p = candidate + wl - 1;  p > candidate;  p--) {
     for (q = p - 1;  (q >= candidate) && ((p - q) < 10);  q--) {
      tmpc = *(q+1);
      *(q+1) = *q;
      *q = tmpc;
      if ((p-q) < 2) continue; // omit swap char
      ns = testsug(wlst, candidate, wl, ns, cpdsuggest, NULL, NULL);
      if (ns == -1) return -1;
    }
    strcpy(candidate, word);
   }   
   return ns;
}

// error is a letter was moved
int SuggestMgr::movechar_utf(char ** wlst, const w_char * word, int wl, int ns, int cpdsuggest)
{
   w_char candidate_utf[MAXSWL];
   char   candidate[MAXSWUTF8L];
   w_char * p;
   w_char * q;
   w_char tmpc;
   // try moving a char
   memcpy (candidate_utf, word, wl * sizeof(w_char));
   for (p = candidate_utf;  p < (candidate_utf + wl);  p++) {
     for (q = p + 1;  (q < (candidate_utf + wl)) && ((q - p) < 10);  q++) {
         tmpc = *(q-1);
         *(q-1) = *q;
         *q = tmpc;
         if ((q-p) < 2) continue; // omit swap char
         u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
         ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
         if (ns == -1) return -1;
     }
     memcpy (candidate_utf, word, wl * sizeof(w_char));
   }
   for (p = candidate_utf + wl - 1;  p > candidate_utf;  p--) {
     for (q = p - 1;  (q >= candidate_utf) && ((p - q) < 10);  q--) {
         tmpc = *(q+1);
         *(q+1) = *q;
         *q = tmpc;
         if ((p-q) < 2) continue; // omit swap char
         u16_u8(candidate, MAXSWUTF8L, candidate_utf, wl);
         ns = testsug(wlst, candidate, strlen(candidate), ns, cpdsuggest, NULL, NULL);
         if (ns == -1) return -1;
     }
     memcpy (candidate_utf, word, wl * sizeof(w_char));
   }
   return ns;   
}

// generate a set of suggestions for very poorly spelled words
int SuggestMgr::ngsuggest(char** wlst, char * w, int ns, HashMgr* pHMgr)
{

  int i, j;
  int lval;
  int sc, scphon;
  int lp, lpphon;
  int nonbmp = 0;

  if (!pHMgr) return ns;

  // exhaustively search through all root words
  // keeping track of the MAX_ROOTS most similar root words
  struct hentry * roots[MAX_ROOTS];
  char * rootsphon[MAX_ROOTS];
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
  scphon = scoresphon[MAX_ROOTS-1];
  
  char w2[MAXWORDUTF8LEN];
  char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }

  char mw[MAXSWUTF8L];
  w_char u8[MAXSWL];
  int nc = strlen(word);
  int n = (utf8) ? u8_u16(u8, MAXSWL, word) : nc;
  
  // set character based ngram suggestion for words with non-BMP Unicode characters
  if (n == -1) {
    utf8 = 0;
    n = nc;
    nonbmp = 1;
  }

  struct hentry* hp = NULL;
  int col = -1;
  phonetable * ph = (pAMgr) ? pAMgr->get_phonetable() : NULL;
  char target[MAXSWUTF8L];
  char candidate[MAXSWUTF8L];
  if (ph) {
    strcpy(candidate, word);
    mkallcap(candidate, csconv);
    phonet(candidate, target, n, *ph);
//    fprintf(stderr, "Tip: %s->%s\n", candidate, target);
  }
  
  while ((hp = pHMgr->walk_hashtable(col, hp))) {
    if ((hp->astr) && (pAMgr) && 
       (TESTAFF(hp->astr, pAMgr->get_forbiddenword(), hp->alen) ||
          TESTAFF(hp->astr, ONLYUPCASEFLAG, hp->alen) ||
          TESTAFF(hp->astr, pAMgr->get_nosuggest(), hp->alen) ||
          TESTAFF(hp->astr, pAMgr->get_onlyincompound(), hp->alen))) continue;

    sc = ngram(3, word, &(hp->word), NGRAM_LONGER_WORSE + NGRAM_LOWERING) +
	leftcommonsubstring(word, &(hp->word));

    // check special pronounciation
    if (hp->var) {
	int sc2 = ngram(3, word, &(hp->word) + hp->blen + 1, NGRAM_LONGER_WORSE + NGRAM_LOWERING) +
	leftcommonsubstring(word, &(hp->word) + hp->blen + 1);
	if (sc2 > sc) sc = sc2;
    }
    
    if (ph && (sc > 2) && (abs(n - (int) hp->clen) <= 3)) {
	char target2[MAXSWUTF8L];
        strcpy(candidate, &(hp->word));
        mkallcap(candidate, csconv);
        phonet(candidate, target2, -1, *ph);
        scphon = 2 * ngram(3, target, target2, NGRAM_LONGER_WORSE);
    }

    if (sc > scores[lp]) {
      scores[lp] = sc;  
      roots[lp] = hp;
      lval = sc;
      for (j=0; j < MAX_ROOTS; j++)
        if (scores[j] < lval) {
          lp = j;
          lval = scores[j];
        }
    }

    if (scphon > scoresphon[lpphon]) {
      scoresphon[lpphon] = scphon;
      rootsphon[lpphon] = &(hp->word);
      lval = scphon;
      for (j=0; j < MAX_ROOTS; j++)
        if (scoresphon[j] < lval) {
          lpphon = j;
          lval = scoresphon[j];
        }
    }
  }

  // find minimum threshhold for a passable suggestion
  // mangle original word three differnt ways
  // and score them to generate a minimum acceptable score
  int thresh = 0;
  for (int sp = 1; sp < 4; sp++) {
     if (utf8) {
       for (int k=sp; k < n; k+=4) *((unsigned short *) u8 + k) = '*';
       u16_u8(mw, MAXSWUTF8L, u8, n);
       thresh = thresh + ngram(n, word, mw, NGRAM_ANY_MISMATCH + NGRAM_LOWERING);
     } else {
       strcpy(mw, word);
       for (int k=sp; k < n; k+=4) *(mw + k) = '*';
       thresh = thresh + ngram(n, word, mw, NGRAM_ANY_MISMATCH + NGRAM_LOWERING);
     }
  }
  thresh = thresh / 3;
  thresh--;

  // now expand affixes on each of these root words and
  // and use length adjusted ngram scores to select
  // possible suggestions
  char * guess[MAX_GUESS];
  char * guessorig[MAX_GUESS];
  int gscore[MAX_GUESS];
  for(i=0;i<MAX_GUESS;i++) {
     guess[i] = NULL;
     guessorig[i] = NULL;
     gscore[i] = -100 * i;
  }

  lp = MAX_GUESS - 1;

  struct guessword * glst;
  glst = (struct guessword *) calloc(MAX_WORDS,sizeof(struct guessword));
  if (! glst) {
    if (nonbmp) utf8 = 1;
    return ns;
  }

  for (i = 0; i < MAX_ROOTS; i++) {
      if (roots[i]) {
        struct hentry * rp = roots[i];
        int nw = pAMgr->expand_rootword(glst, MAX_WORDS, &(rp->word), rp->blen,
            	    rp->astr, rp->alen, word, nc, 
                    ((rp->var) ? &(rp->word) + rp->blen + 1 : NULL));

        for (int k = 0; k < nw ; k++) {
           sc = ngram(n, word, glst[k].word, NGRAM_ANY_MISMATCH + NGRAM_LOWERING) +
               leftcommonsubstring(word, glst[k].word);

           if ((sc > thresh)) {
              if (sc > gscore[lp]) {
                 if (guess[lp]) {
                    free (guess[lp]);
                    if (guessorig[lp]) {
                	free(guessorig[lp]);
                	guessorig[lp] = NULL;
            	    }
                 }
                 gscore[lp] = sc;
                 guess[lp] = glst[k].word;
                 guessorig[lp] = glst[k].orig;
                 lval = sc;
                 for (j=0; j < MAX_GUESS; j++)
                    if (gscore[j] < lval) {
                       lp = j;
                       lval = gscore[j];
                    }
              } else { 
                free(glst[k].word);
                if (glst[k].orig) free(glst[k].orig);
              }
           } else {
        	free(glst[k].word);
                if (glst[k].orig) free(glst[k].orig);
           }
        }
      }
  }
  free(glst);

  // now we are done generating guesses
  // sort in order of decreasing score
  
  
  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);
  if (ph) bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // weight suggestions with a similarity index, based on
  // the longest common subsequent algorithm and resort

  int is_swap;
  for (i=0; i < MAX_GUESS; i++) {
      if (guess[i]) {
        // lowering guess[i]
        char gl[MAXSWUTF8L];
        int len;
        if (utf8) {
          w_char _w[MAXSWL];
          len = u8_u16(_w, MAXSWL, guess[i]);
          mkallsmall_utf(_w, len, langnum);
          u16_u8(gl, MAXSWUTF8L, _w, len);
        } else {
          strcpy(gl, guess[i]);
          mkallsmall(gl, csconv);
          len = strlen(guess[i]);
        }

        int _lcs = lcslen(word, gl);

        // same characters with different casing
        if ((n == len) && (n == _lcs)) {
            gscore[i] += 2000;
            break;
        }
        
        // heuristic weigthing of ngram scores
        gscore[i] +=
          // length of longest common subsequent minus length difference
          2 * _lcs - abs((int) (n - len)) +
          // weight length of the left common substring
          leftcommonsubstring(word, gl) +
          // weight equal character positions
          ((_lcs == commoncharacterpositions(word, gl, &is_swap)) ? 1: 0) +
          // swap character (not neighboring)
          ((is_swap) ? 1000 : 0);
      }
  }

  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);

// phonetic version
  if (ph) for (i=0; i < MAX_ROOTS; i++) {
      if (rootsphon[i]) {
        // lowering rootphon[i]
        char gl[MAXSWUTF8L];
        int len;
        if (utf8) {
          w_char _w[MAXSWL];
          len = u8_u16(_w, MAXSWL, rootsphon[i]);
          mkallsmall_utf(_w, len, langnum);
          u16_u8(gl, MAXSWUTF8L, _w, len);
        } else {
          strcpy(gl, rootsphon[i]);
          mkallsmall(gl, csconv);
          len = strlen(rootsphon[i]);
        }

        // heuristic weigthing of ngram scores
        scoresphon[i] += 2 * lcslen(word, gl) - abs((int) (n - len)) +
          // weight length of the left common substring
          leftcommonsubstring(word, gl);
      }
  }

  if (ph) bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // copy over
  int oldns = ns;

  int same = 0;
  for (i=0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      if ((ns < oldns + maxngramsugs) && (ns < maxSug) && (!same || (gscore[i] > 1000))) {
        int unique = 1;
        // leave only excellent suggestions, if exists
        if (gscore[i] > 1000) same = 1;
        for (j = 0; j < ns; j++) {
          // don't suggest previous suggestions or a previous suggestion with prefixes or affixes
          if ((!guessorig[i] && strstr(guess[i], wlst[j])) ||
	     (guessorig[i] && strstr(guessorig[i], wlst[j])) ||
            // check forbidden words
            !checkword(guess[i], strlen(guess[i]), 0, NULL, NULL)) unique = 0;
        }
        if (unique) {
    	    wlst[ns++] = guess[i];
    	    if (guessorig[i]) {
    		free(guess[i]);
    		wlst[ns-1] = guessorig[i];
    	    }
    	} else {
    	    free(guess[i]);
    	    if (guessorig[i]) free(guessorig[i]);
    	}
      } else {
        free(guess[i]);
    	if (guessorig[i]) free(guessorig[i]);
      }
    }
  }

  oldns = ns;
  if (ph) for (i=0; i < MAX_ROOTS; i++) {
    if (rootsphon[i]) {
      if ((ns < oldns + MAXPHONSUGS) && (ns < maxSug)) {
        int unique = 1;
        for (j = 0; j < ns; j++) {
          // don't suggest previous suggestions or a previous suggestion with prefixes or affixes
          if (strstr(rootsphon[i], wlst[j]) || 
            // check forbidden words
            !checkword(rootsphon[i], strlen(rootsphon[i]), 0, NULL, NULL)) unique = 0;
        }
        if (unique) wlst[ns++] = mystrdup(rootsphon[i]);
      }
    }
  }

  if (nonbmp) utf8 = 1;
  return ns;
}


// see if a candidate suggestion is spelled correctly
// needs to check both root words and words with affixes

// obsolote MySpell-HU modifications:
// return value 2 and 3 marks compounding with hyphen (-)
// `3' marks roots without suffix
int SuggestMgr::checkword(const char * word, int len, int cpdsuggest, int * timer, clock_t * timelimit)
{
  struct hentry * rv=NULL;
  int nosuffix = 0;

  // check time limit
  if (timer) {
    (*timer)--;
    if (!(*timer) && timelimit) {
      if ((clock() - *timelimit) > TIMELIMIT) return 0;
      *timer = MAXPLUSTIMER;
    }
  }
  
  if (pAMgr) { 
    if (cpdsuggest==1) {
      if (pAMgr->get_compound()) {
        rv = pAMgr->compound_check(word,len,0,0,0,0,NULL,0,NULL,NULL,1);
        if (rv) return 3; // XXX obsolote categorisation
        }
        return 0;
    }

    rv = pAMgr->lookup(word);

    if (rv) {
        if ((rv->astr) && (TESTAFF(rv->astr,pAMgr->get_forbiddenword(),rv->alen)
               || TESTAFF(rv->astr,pAMgr->get_nosuggest(),rv->alen))) return 0;
        while (rv) {
    	    if (rv->astr && (TESTAFF(rv->astr,pAMgr->get_pseudoroot(),rv->alen) ||
                  TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
            TESTAFF(rv->astr,pAMgr->get_onlyincompound(),rv->alen))) {
        	rv = rv->next_homonym;
    	    } else break;
    	}
    } else rv = pAMgr->prefix_check(word, len, 0); // only prefix, and prefix + suffix XXX
    
    if (rv) {
        nosuffix=1;
    } else {
        rv = pAMgr->suffix_check(word, len, 0, NULL, NULL, 0, NULL); // only suffix
    }

    if (!rv && pAMgr->have_contclass()) {
        rv = pAMgr->suffix_check_twosfx(word, len, 0, NULL, FLAG_NULL);
        if (!rv) rv = pAMgr->prefix_check_twosfx(word, len, 1, FLAG_NULL);
    }

    // check forbidden words
    if ((rv) && (rv->astr) && (TESTAFF(rv->astr,pAMgr->get_forbiddenword(),rv->alen) ||
      TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
      TESTAFF(rv->astr,pAMgr->get_nosuggest(),rv->alen) ||
      TESTAFF(rv->astr,pAMgr->get_onlyincompound(),rv->alen))) return 0;

    if (rv) { // XXX obsolote    
      if ((pAMgr->get_compoundflag()) && 
          TESTAFF(rv->astr, pAMgr->get_compoundflag(), rv->alen)) return 2 + nosuffix; 
      return 1;
    }
  }
  return 0;
}

int SuggestMgr::check_forbidden(const char * word, int len)
{
  struct hentry * rv = NULL;

  if (pAMgr) { 
    rv = pAMgr->lookup(word);
    if (rv && rv->astr && (TESTAFF(rv->astr,pAMgr->get_pseudoroot(),rv->alen) ||
        TESTAFF(rv->astr,pAMgr->get_onlyincompound(),rv->alen))) rv = NULL;
    if (!(pAMgr->prefix_check(word,len,1)))
        rv = pAMgr->suffix_check(word,len, 0, NULL, NULL, 0, NULL); // prefix+suffix, suffix
    // check forbidden words
    if ((rv) && (rv->astr) && TESTAFF(rv->astr,pAMgr->get_forbiddenword(),rv->alen)) return 1;
   }
    return 0;
}

#ifdef HUNSPELL_EXPERIMENTAL
// suggest stems, XXX experimental code
int SuggestMgr::suggest_stems(char*** slst, const char * w, int nsug)
{
    char buf[MAXSWUTF8L];
    char ** wlst;    
    int prevnsug = nsug;

  char w2[MAXWORDUTF8LEN];
  const char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }

    if (*slst) {
        wlst = *slst;
    } else {
        wlst = (char **) calloc(maxSug, sizeof(char *));
        if (wlst == NULL) return -1;
    }
    // perhaps there are a fix stem in the dictionary
    if ((nsug < maxSug) && (nsug > -1)) {
    
    nsug = fixstems(wlst, word, nsug);
    if (nsug == prevnsug) {
        char * s = mystrdup(word);
        char * p = s + strlen(s);
        while ((*p != '-') && (p != s)) p--;
        if (*p == '-') {
            *p = '\0';
            nsug = fixstems(wlst, s, nsug);
            if ((nsug == prevnsug) && (nsug < maxSug) && (nsug >= 0)) {
                char * t;
                buf[0] = '\0';
                for (t = s; (t[0] != '\0') && ((t[0] >= '0') || (t[0] <= '9')); t++); // is a number?
                if (*t != '\0') strcpy(buf, "# ");
                strcat(buf, s);
                wlst[nsug] = mystrdup(buf);
                if (wlst[nsug] == NULL) return -1;
                nsug++;
            }
            p++;
            nsug = fixstems(wlst, p, nsug);
        }

        free(s);
    }
    }
    
    if (nsug < 0) {
       for (int i=0;i<maxSug; i++)
         if (wlst[i] != NULL) free(wlst[i]);
         free(wlst);
       return -1;
    }

    *slst = wlst;
    return nsug;
}


// there are fix stems in dictionary
int SuggestMgr::fixstems(char ** wlst, const char * word, int ns)
{
    char buf[MAXSWUTF8L];
    char prefix[MAXSWUTF8L] = "";

    int dicstem = 1; // 0 = lookup, 1= affix, 2 = compound
    int cpdindex = 0;
    struct hentry * rv = NULL;

    int wl = strlen(word);
    int cmpdstemnum;
    int cmpdstem[MAXCOMPOUND];

    if (pAMgr) { 
        rv = pAMgr->lookup(word);
        if (rv) {
            dicstem = 0;
        } else {
            // try stripping off affixes 
            rv = pAMgr->affix_check(word, wl);

            // else try check compound word
            if (!rv && pAMgr->get_compound()) {
                rv = pAMgr->compound_check(word, wl,
                     0, 0, 100, 0, NULL, 0, &cmpdstemnum, cmpdstem,1);

                if (rv) {
                    dicstem = 2;
                    for (int j = 0; j < cmpdstemnum; j++) {
                        cpdindex += cmpdstem[j];
                    }
                    if(! (pAMgr->lookup(word + cpdindex)))
                        pAMgr->affix_check(word + cpdindex, wl - cpdindex); // for prefix
                }
            }


            if (pAMgr->get_prefix()) {
                strcpy(prefix, pAMgr->get_prefix());
            }

            // XXX obsolete, will be a general solution for stemming
            if ((prefix) && (strncmp(prefix, "leg", 3)==0)) prefix[0] = '\0'; // (HU)       
        }

    }



    if ((rv) && (ns < maxSug)) {
    
        // check fixstem flag and not_valid_stem flag
        // first word
        if ((ns < maxSug) && (dicstem < 2)) { 
            strcpy(buf, prefix);
            if ((dicstem > 0) && pAMgr->get_derived()) {
                // XXX obsolote
                   if (strlen(prefix) == 1) {
                        strcat(buf, (pAMgr->get_derived()) + 1);
                   } else {
                        strcat(buf, pAMgr->get_derived());
                   }
                } else {
                        // special stem in affix description
                        const char * wordchars = pAMgr->get_wordchars();
                        if (rv->description && 
                           (strchr(wordchars, *(rv->description)))) {
                           char * desc = (rv->description) + 1;
                           while (strchr(wordchars, *desc)) desc++;
                           strncat(buf, rv->description, desc - (rv->description));
                        } else {
                            strcat(buf, rv->word);
                        }
                }
            wlst[ns] = mystrdup(buf);
            if (wlst[ns] == NULL) return -1;
            ns++;
        }

        if (dicstem == 2) {

            // compound stem

//          if (rv->astr && (strchr(rv->astr, '0') == NULL)) {
            if (rv->astr) {
                strcpy(buf, word);
                buf[cpdindex] = '\0';
                if (prefix) strcat(buf, prefix);
                if (pAMgr->get_derived()) {
                        strcat(buf, pAMgr->get_derived());
                } else {
                        // special stem in affix description
                        const char * wordchars = pAMgr->get_wordchars();
                        if (rv->description && 
                           (strchr(wordchars, *(rv->description)))) {
                           char * desc = (rv->description) + 1;
                           while (strchr(wordchars, *desc)) desc++;
                           strncat(buf, rv->description, desc - (rv->description));
                        } else {
                            strcat(buf, rv->word);
                        }
                }
                if (ns < maxSug) {
                    wlst[ns] = mystrdup(buf);
                    if (wlst[ns] == NULL) return -1;
                    ns++;
                }
            }
        }
    }
    return ns;
}

// suggest possible stems
int SuggestMgr::suggest_pos_stems(char*** slst, const char * w, int nsug)
{
    char ** wlst;    

    struct hentry * rv = NULL;

  char w2[MAXSWUTF8L];
  const char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }

    int wl = strlen(word);


    if (*slst) {
        wlst = *slst;
    } else {
        wlst = (char **) calloc(maxSug, sizeof(char *));
        if (wlst == NULL) return -1;
    }

    rv = pAMgr->suffix_check(word, wl, 0, NULL, wlst, maxSug, &nsug);

    // delete dash from end of word
    if (nsug > 0) {
        for (int j=0; j < nsug; j++) {
            if (wlst[j][strlen(wlst[j]) - 1] == '-') wlst[j][strlen(wlst[j]) - 1] = '\0';
        }
    }

    *slst = wlst;
    return nsug;
}


char * SuggestMgr::suggest_morph(const char * w)
{
    char result[MAXLNLEN];
    char * r = (char *) result;
    char * st;

    struct hentry * rv = NULL;

    *result = '\0';

    if (! pAMgr) return NULL;

  char w2[MAXSWUTF8L];
  const char * word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    strcpy(w2, w);
    if (utf8) reverseword_utf(w2); else reverseword(w2);
    word = w2;
  }

    rv = pAMgr->lookup(word);
    
    while (rv) {
        if ((!rv->astr) || !(TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
            TESTAFF(rv->astr, pAMgr->get_pseudoroot(), rv->alen) ||
            TESTAFF(rv->astr,pAMgr->get_onlyincompound(),rv->alen))) {
            if (rv->description && ((!rv->astr) || 
                !TESTAFF(rv->astr, pAMgr->get_lemma_present(), rv->alen)))
                    strcat(result, word);
            if (rv->description) strcat(result, rv->description);
            strcat(result, "\n");
        }
        rv = rv->next_homonym;
    }
    
    st = pAMgr->affix_check_morph(word,strlen(word));
    if (st) {
        strcat(result, st);
        free(st);
    }

    if (pAMgr->get_compound() && (*result == '\0'))
        pAMgr->compound_check_morph(word, strlen(word),
                     0, 0, 100, 0,NULL, 0, &r, NULL);
    
    return (*result) ? mystrdup(line_uniq(delete_zeros(result))) : NULL;
}

char * SuggestMgr::suggest_morph_for_spelling_error(const char * word)
{
    char * p = NULL;
    char ** wlst = (char **) calloc(maxSug, sizeof(char *));
    if (!**wlst) return NULL;
    // we will use only the first suggestion
    for (int i = 0; i < maxSug - 1; i++) wlst[i] = "";
    int ns = suggest(&wlst, word, maxSug - 1);
    if (ns == maxSug) {
        p = suggest_morph(wlst[maxSug - 1]);
        free(wlst[maxSug - 1]);
    }
    if (wlst) free(wlst);
    return p;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE


// generate an n-gram score comparing s1 and s2
int SuggestMgr::ngram(int n, char * s1, const char * s2, int opt)
{
  int nscore = 0;
  int ns;
  int l1;
  int l2;

  if (utf8) {
    w_char su1[MAXSWL];
    w_char su2[MAXSWL];
    l1 = u8_u16(su1, MAXSWL, s1);
    l2 = u8_u16(su2, MAXSWL, s2);
    if ((l2 <= 0) || (l1 == -1)) return 0;
    // lowering dictionary word
    if (opt & NGRAM_LOWERING) mkallsmall_utf(su2, l2, langnum);
    for (int j = 1; j <= n; j++) {
      ns = 0;
      for (int i = 0; i <= (l1-j); i++) {
        for (int l = 0; l <= (l2-j); l++) {
            int k;
            for (k = 0; (k < j); k++) {
              w_char * c1 = su1 + i + k;
              w_char * c2 = su2 + l + k;
              if ((c1->l != c2->l) || (c1->h != c2->h)) break;
            }
            if (k == j) {
                ns++;
                break;
            }
        }
      }
      nscore = nscore + ns;
      if (ns < 2) break;
    }
  } else {  
    char t[MAXSWUTF8L];
    l1 = strlen(s1);
    l2 = strlen(s2);
    if (l2 == 0) return 0;
    strcpy(t, s2);
    if (opt & NGRAM_LOWERING) mkallsmall(t, csconv);
    for (int j = 1; j <= n; j++) {
      ns = 0;
      for (int i = 0; i <= (l1-j); i++) {
        char c = *(s1 + i + j);
        *(s1 + i + j) = '\0';
        if (strstr(t,(s1+i))) ns++;
        *(s1 + i + j ) = c;
      }
      nscore = nscore + ns;
      if (ns < 2) break;
    }
  }
  
  ns = 0;
  if (opt & NGRAM_LONGER_WORSE) ns = (l2-l1)-2;
  if (opt & NGRAM_ANY_MISMATCH) ns = abs(l2-l1)-2;
  ns = (nscore - ((ns > 0) ? ns : 0));
  return ns;
}

// length of the left common substring of s1 and (decapitalised) s2
int SuggestMgr::leftcommonsubstring(char * s1, const char * s2) {
  if (utf8) {
    w_char su1[MAXSWL];
    w_char su2[MAXSWL];
    // decapitalize dictionary word
    if (complexprefixes) {
      int l1 = u8_u16(su1, MAXSWL, s1);
      int l2 = u8_u16(su2, MAXSWL, s2);
      if (*((short *)su1+l1-1) == *((short *)su2+l2-1)) return 1;
    } else {
      int i;
      u8_u16(su1, 1, s1);
      u8_u16(su2, 1, s2);
      unsigned short idx = (su2->h << 8) + su2->l;
      if (*((short *)su1) != *((short *)su2) &&
         (*((unsigned short *)su1) != unicodetolower(idx, langnum))) return 0;
      int l1 = u8_u16(su1, MAXSWL, s1);
      int l2 = u8_u16(su2, MAXSWL, s2);
      for(i = 1; (i < l1) && (i < l2) &&
         (*((short *)(su1 + i)) == *((short *)(su2 + i))); i++);
      return i;
    }
  } else {
    if (complexprefixes) {
      int l1 = strlen(s1);
      int l2 = strlen(s2);
      if (*(s2+l1-1) == *(s2+l2-1)) return 1;
    } else {
      char * olds = s1;
      // decapitalise dictionary word
      if ((*s1 != *s2) && (*s1 != csconv[((unsigned char)*s2)].clower)) return 0;
      do {
        s1++; s2++;
      } while ((*s1 == *s2) && (*s1 != '\0'));
      return s1 - olds;
    }
  }
  return 0;
}

int SuggestMgr::commoncharacterpositions(char * s1, const char * s2, int * is_swap) {
  int num = 0;
  int diff = 0;
  int diffpos[2];
  *is_swap = 0;
  if (utf8) {
    w_char su1[MAXSWL];
    w_char su2[MAXSWL];
    int l1 = u8_u16(su1, MAXSWL, s1);
    int l2 = u8_u16(su2, MAXSWL, s2);
    // decapitalize dictionary word
    if (complexprefixes) {
      mkallsmall_utf(su2+l2-1, 1, langnum);
    } else {
      mkallsmall_utf(su2, 1, langnum);
    }
    for (int i = 0; (i < l1) && (i < l2); i++) {
      if (((short *) su1)[i] == ((short *) su2)[i]) {
        num++;
      } else {
        if (diff < 2) diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (l1 == l2) &&
        (((short *) su1)[diffpos[0]] == ((short *) su2)[diffpos[1]]) &&
        (((short *) su1)[diffpos[1]] == ((short *) su2)[diffpos[0]])) *is_swap = 1;
  } else {
    int i;
    char t[MAXSWUTF8L];
    strcpy(t, s2);
    // decapitalize dictionary word
    if (complexprefixes) {
      int l2 = strlen(t);
      *(t+l2-1) = csconv[((unsigned char)*(t+l2-1))].clower;
    } else {
      mkallsmall(t, csconv);
    }
    for (i = 0; (*(s1+i) != 0) && (*(t+i) != 0); i++) {
      if (*(s1+i) == *(t+i)) {
        num++;
      } else {
        if (diff < 2) diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (*(s1+i) == 0) && (*(t+i) == 0) &&
      (*(s1+diffpos[0]) == *(t+diffpos[1])) &&
      (*(s1+diffpos[1]) == *(t+diffpos[0]))) *is_swap = 1;
  }
  return num;
}

int SuggestMgr::mystrlen(const char * word) {
  if (utf8) {
    w_char w[MAXSWL];
    return u8_u16(w, MAXSWL, word);
  } else return strlen(word);
}

// sort in decreasing order of score
void SuggestMgr::bubblesort(char** rword, char** rword2, int* rsc, int n )
{
      int m = 1;
      while (m < n) {
          int j = m;
          while (j > 0) {
            if (rsc[j-1] < rsc[j]) {
                int sctmp = rsc[j-1];
                char * wdtmp = rword[j-1];
                rsc[j-1] = rsc[j];
                rword[j-1] = rword[j];
                rsc[j] = sctmp;
                rword[j] = wdtmp;
                if (rword2) {
            	    wdtmp = rword2[j-1];
            	    rword2[j-1] = rword2[j];
            	    rword2[j] = wdtmp;
                }
                j--;
            } else break;
          }
          m++;
      }
      return;
}

// longest common subsequence
void SuggestMgr::lcs(const char * s, const char * s2, int * l1, int * l2, char ** result) {
  int n, m;
  w_char su[MAXSWL];
  w_char su2[MAXSWL];
  char * b;
  char * c;
  int i;
  int j;
  if (utf8) {
    m = u8_u16(su, MAXSWL, s);
    n = u8_u16(su2, MAXSWL, s2);
  } else {
    m = strlen(s);
    n = strlen(s2);
  }
  c = (char *) malloc((m + 1) * (n + 1));
  b = (char *) malloc((m + 1) * (n + 1));
  if (!c || !b) {
    if (c) free(c);
    if (b) free(b);
    *result = NULL;
    return;
  }
  for (i = 1; i <= m; i++) c[i*(n+1)] = 0;
  for (j = 0; j <= n; j++) c[j] = 0;
  for (i = 1; i <= m; i++) {
    for (j = 1; j <= n; j++) {
      if ((utf8) && (*((short *) su+i-1) == *((short *)su2+j-1))
          || (!utf8) && ((*(s+i-1)) == (*(s2+j-1)))) {
        c[i*(n+1) + j] = c[(i-1)*(n+1) + j-1]+1;
        b[i*(n+1) + j] = LCS_UPLEFT;
      } else if (c[(i-1)*(n+1) + j] >= c[i*(n+1) + j-1]) {
        c[i*(n+1) + j] = c[(i-1)*(n+1) + j];
        b[i*(n+1) + j] = LCS_UP;
      } else {
        c[i*(n+1) + j] = c[i*(n+1) + j-1];
        b[i*(n+1) + j] = LCS_LEFT;
      }
    }
  }
  *result = b;
  free(c);
  *l1 = m;
  *l2 = n;
}

int SuggestMgr::lcslen(const char * s, const char* s2) {
  int m;
  int n;
  int i;
  int j;
  char * result;
  int len = 0;
  lcs(s, s2, &m, &n, &result);
  if (!result) return 0;
  i = m;
  j = n;
  while ((i != 0) && (j != 0)) {
    if (result[i*(n+1) + j] == LCS_UPLEFT) {
      len++;
      i--;
      j--;
    } else if (result[i*(n+1) + j] == LCS_UP) {
      i--;
    } else j--;
  }
  free(result);
  return len;
}
