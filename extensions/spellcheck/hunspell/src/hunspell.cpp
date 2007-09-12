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
#else
#include <stdlib.h> 
#include <string.h>
#include <stdio.h> 
#endif

#include "hunspell.hxx"
#include "hunspell.h"

#ifndef MOZILLA_CLIENT
#ifndef W32
using namespace std;
#endif
#endif

Hunspell::Hunspell(const char * affpath, const char * dpath)
{
    encoding = NULL;
    csconv = NULL;
    utf8 = 0;
    complexprefixes = 0;

    /* first set up the hash manager */
    pHMgr = new HashMgr(dpath, affpath);

    /* next set up the affix manager */
    /* it needs access to the hash manager lookup methods */
    pAMgr = new AffixMgr(affpath,pHMgr);

    /* get the preferred try string and the dictionary */
    /* encoding from the Affix Manager for that dictionary */
    char * try_string = pAMgr->get_try_string();
    encoding = pAMgr->get_encoding();
    csconv = get_current_cs(encoding);
    langnum = pAMgr->get_langnum();
    utf8 = pAMgr->get_utf8();
    complexprefixes = pAMgr->get_complexprefixes();
    wordbreak = pAMgr->get_breaktable();

    /* and finally set up the suggestion manager */
    pSMgr = new SuggestMgr(try_string, MAXSUGGESTION, pAMgr);
    if (try_string) free(try_string);

}

Hunspell::~Hunspell()
{
    if (pSMgr) delete pSMgr;
    if (pAMgr) delete pAMgr;
    if (pHMgr) delete pHMgr;
    pSMgr = NULL;
    pAMgr = NULL;
    pHMgr = NULL;
#ifdef MOZILLA_CLIENT
    delete csconv;
#endif
    csconv= NULL;
    if (encoding) free(encoding);
    encoding = NULL;
}


// make a copy of src at destination while removing all leading
// blanks and removing any trailing periods after recording
// their presence with the abbreviation flag
// also since already going through character by character, 
// set the capitalization type
// return the length of the "cleaned" (and UTF-8 encoded) word

int Hunspell::cleanword2(char * dest, const char * src, 
    w_char * dest_utf, int * nc, int * pcaptype, int * pabbrev)
{ 
   unsigned char * p = (unsigned char *) dest;
   const unsigned char * q = (const unsigned char * ) src;

   // first skip over any leading blanks
   while ((*q != '\0') && (*q == ' ')) q++;
   
   // now strip off any trailing periods (recording their presence)
   *pabbrev = 0;
   int nl = strlen((const char *)q);
   while ((nl > 0) && (*(q+nl-1)=='.')) {
       nl--;
       (*pabbrev)++;
   }
   
   // if no characters are left it can't be capitalized
   if (nl <= 0) { 
       *pcaptype = NOCAP;
       *p = '\0';
       return 0;
   }
   
   strncpy(dest, (char *) q, nl);
   *(dest + nl) = '\0';
   nl = strlen(dest);
   if (utf8) {
      *nc = u8_u16(dest_utf, MAXWORDLEN, dest);
      // don't check too long words
      if (*nc >= MAXWORDLEN) return 0;
      if (*nc == -1) { // big Unicode character (non BMP area)
         *pcaptype = NOCAP;
         return nl;
      }
     *pcaptype = get_captype_utf8(dest_utf, *nc, langnum);
   } else {
     *pcaptype = get_captype(dest, nl, csconv);
     *nc = nl;
   }
   return nl;
} 

#ifdef HUNSPELL_EXPERIMENTAL
int Hunspell::cleanword(char * dest, const char * src, 
    int * pcaptype, int * pabbrev)
{ 
   unsigned char * p = (unsigned char *) dest;
   const unsigned char * q = (const unsigned char * ) src;
   int firstcap = 0;

   // first skip over any leading blanks
   while ((*q != '\0') && (*q == ' ')) q++;
   
   // now strip off any trailing periods (recording their presence)
   *pabbrev = 0;
   int nl = strlen((const char *)q);
   while ((nl > 0) && (*(q+nl-1)=='.')) {
       nl--;
       (*pabbrev)++;
   }
   
   // if no characters are left it can't be capitalized
   if (nl <= 0) { 
       *pcaptype = NOCAP;
       *p = '\0';
       return 0;
   }

   // now determine the capitalization type of the first nl letters
   int ncap = 0;
   int nneutral = 0;
   int nc = 0;

   if (!utf8) {
      while (nl > 0) {
         nc++;
         if (csconv[(*q)].ccase) ncap++;
         if (csconv[(*q)].cupper == csconv[(*q)].clower) nneutral++;
         *p++ = *q++;
         nl--;
      }
      // remember to terminate the destination string
      *p = '\0';
      firstcap = csconv[(unsigned char)(*dest)].ccase;
   } else {
      unsigned short idx;
      w_char t[MAXWORDLEN];
      nc = u8_u16(t, MAXWORDLEN, src);
      for (int i = 0; i < nc; i++) {
         idx = (t[i].h << 8) + t[i].l;
         unsigned short low = unicodetolower(idx, langnum);
         if (idx != low) ncap++;
         if (unicodetoupper(idx, langnum) == low) nneutral++;
      }
      u16_u8(dest, MAXWORDUTF8LEN, t, nc);
      if (ncap) {
         idx = (t[0].h << 8) + t[0].l;
         firstcap = (idx != unicodetolower(idx, langnum));
      }
   }

   // now finally set the captype
   if (ncap == 0) {
        *pcaptype = NOCAP;
   } else if ((ncap == 1) && firstcap) {
        *pcaptype = INITCAP;
   } else if ((ncap == nc) || ((ncap + nneutral) == nc)){
        *pcaptype = ALLCAP;
   } else if ((ncap > 1) && firstcap) {
        *pcaptype = HUHINITCAP;
   } else {
        *pcaptype = HUHCAP;
   }
   return strlen(dest);
} 
#endif       

void Hunspell::mkallcap(char * p)
{
  if (utf8) {
      w_char u[MAXWORDLEN];
      int nc = u8_u16(u, MAXWORDLEN, p);
      unsigned short idx;
      for (int i = 0; i < nc; i++) {
         idx = (u[i].h << 8) + u[i].l;
         if (idx != unicodetoupper(idx, langnum)) {
            u[i].h = (unsigned char) (unicodetoupper(idx, langnum) >> 8);
            u[i].l = (unsigned char) (unicodetoupper(idx, langnum) & 0x00FF);
         }
      }
      u16_u8(p, MAXWORDUTF8LEN, u, nc);
  } else {
    while (*p != '\0') { 
        *p = csconv[((unsigned char) *p)].cupper;
        p++;
    }
  }
}

int Hunspell::mkallcap2(char * p, w_char * u, int nc)
{
  if (utf8) {
      unsigned short idx;
      for (int i = 0; i < nc; i++) {
         idx = (u[i].h << 8) + u[i].l;
         unsigned short up = unicodetoupper(idx, langnum);
         if (idx != up) {
            u[i].h = (unsigned char) (up >> 8);
            u[i].l = (unsigned char) (up & 0x00FF);
         }
      }
      u16_u8(p, MAXWORDUTF8LEN, u, nc);
      return strlen(p);  
  } else {
    while (*p != '\0') { 
        *p = csconv[((unsigned char) *p)].cupper;
        p++;
    }
  }
  return nc;
}


void Hunspell::mkallsmall(char * p)
{
    while (*p != '\0') { 
        *p = csconv[((unsigned char) *p)].clower;
        p++;
    }
}

int Hunspell::mkallsmall2(char * p, w_char * u, int nc)
{
  if (utf8) {
      unsigned short idx;
      for (int i = 0; i < nc; i++) {
         idx = (u[i].h << 8) + u[i].l;
         unsigned short low = unicodetolower(idx, langnum);
         if (idx != low) {
            u[i].h = (unsigned char) (low >> 8);
            u[i].l = (unsigned char) (low & 0x00FF);
         }
      }
      u16_u8(p, MAXWORDUTF8LEN, u, nc);
      return strlen(p);
  } else {
    while (*p != '\0') { 
        *p = csconv[((unsigned char) *p)].clower;
        p++;
    }
  }
  return nc;
}

// convert UTF-8 sharp S codes to latin 1
char * Hunspell::sharps_u8_l1(char * dest, char * source) {
    char * p = dest;
    *p = *source;
    for (p++, source++; *(source - 1); p++, source++) {
        *p = *source;
        if (*source == '\x9F') *--p = '\xDF';
    }
    return dest;
}

// recursive search for right ss - sharp s permutations
hentry * Hunspell::spellsharps(char * base, char * pos, int n,
        int repnum, char * tmp, int * info, char **root) {
    pos = strstr(pos, "ss");
    if (pos && (n < MAXSHARPS)) {
        *pos = '\xC3';
        *(pos + 1) = '\x9F';
        hentry * h = spellsharps(base, pos + 2, n + 1, repnum + 1, tmp, info, root);
        if (h) return h;
        *pos = 's';
        *(pos + 1) = 's';
        h = spellsharps(base, pos + 2, n + 1, repnum, tmp, info, root);
        if (h) return h;
    } else if (repnum > 0) {
        if (utf8) return checkword(base, info, root);
        return checkword(sharps_u8_l1(tmp, base), info, root);
    }
    return NULL;
}

int Hunspell::is_keepcase(const hentry * rv) {
    return pAMgr && rv->astr && pAMgr->get_keepcase() &&
        TESTAFF(rv->astr, pAMgr->get_keepcase(), rv->alen);
}

/* insert a word to beginning of the suggestion array and return ns */
int Hunspell::insert_sug(char ***slst, char * word, int ns) {
    if (ns == MAXSUGGESTION) {
        ns--;
        free((*slst)[ns]);
    }
    for (int k = ns; k > 0; k--) (*slst)[k] = (*slst)[k - 1];
    (*slst)[0] = mystrdup(word);
    return ns + 1;
}

int Hunspell::spell(const char * word, int * info, char ** root)
{
  struct hentry * rv=NULL;
  // need larger vector. For example, Turkish capital letter I converted a
  // 2-byte UTF-8 character (dotless i) by mkallsmall.
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  w_char unicw[MAXWORDLEN];
  int nc = strlen(word);
  int wl2 = 0;
  if (utf8) {
    if (nc >= MAXWORDUTF8LEN) return 0;
  } else {
    if (nc >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  int wl = cleanword2(cw, word, unicw, &nc, &captype, &abbv);
  int info2 = 0;
  if (wl == 0) return 1;
  if (root) *root = NULL;

  // allow numbers with dots and commas (but forbid double separators: "..", ",," etc.)
  enum { NBEGIN, NNUM, NSEP };
  int nstate = NBEGIN;
  int i;

  for (i = 0; (i < wl); i++) {
    if ((cw[i] <= '9') && (cw[i] >= '0')) {
        nstate = NNUM;
    } else if ((cw[i] == ',') || (cw[i] == '.') || (cw[i] == '-')) {
        if ((nstate == NSEP) || (i == 0)) break;
        nstate = NSEP;
    } else break;
  }
  if ((i == wl) && (nstate == NNUM)) return 1;
  if (!info) info = &info2; else *info = 0;

  // LANG_hu section: number(s) + (percent or degree) with suffixes
  if (langnum == LANG_hu) {
    if ((nstate == NNUM) && ((cw[i] == '%') || ((!utf8 && (cw[i] == '\xB0')) ||
        (utf8 && (strncmp(cw + i, "\xC2\xB0", 2)==0))))
               && checkword(cw + i, info, root)) return 1;
  }
  // END of LANG_hu section

  switch(captype) {
     case HUHCAP: 
     case HUHINITCAP: 
     case NOCAP: { 
            rv = checkword(cw, info, root);
            if ((abbv) && !(rv)) {
                memcpy(wspace,cw,wl);
                *(wspace+wl) = '.';
                *(wspace+wl+1) = '\0';
                rv = checkword(wspace, info, root);
            }
            break;
         }
     case ALLCAP: {
            rv = checkword(cw, info, root);
            if (rv) break;
            if (abbv) {
                memcpy(wspace,cw,wl);
                *(wspace+wl) = '.';
                *(wspace+wl+1) = '\0';
                rv = checkword(wspace, info, root);
                if (rv) break;
            }
            // Spec. prefix handling for Catalan, French, Italian:
	    // prefixes separated by apostrophe (SANT'ELIA -> Sant'+Elia).
            if (pAMgr && strchr(cw, '\'')) {
                wl = mkallsmall2(cw, unicw, nc);
        	char * apostrophe = strchr(cw, '\'');
                if (utf8) {
            	    w_char tmpword[MAXWORDLEN];
            	    *apostrophe = '\0';
            	    wl2 = u8_u16(tmpword, MAXWORDLEN, cw);
            	    *apostrophe = '\'';
		    if (wl2 < nc) {
		        mkinitcap2(apostrophe + 1, unicw + wl2 + 1, nc - wl2 - 1);
			rv = checkword(cw, info, root);
			if (rv) break;
		    }
                } else {
		    mkinitcap2(apostrophe + 1, unicw, nc);
		    rv = checkword(cw, info, root);
		    if (rv) break;
		}
		mkinitcap2(cw, unicw, nc);
		rv = checkword(cw, info, root);
		if (rv) break;
            }
            if (pAMgr && pAMgr->get_checksharps() && strstr(cw, "SS")) {
                char tmpword[MAXWORDUTF8LEN];
                wl = mkallsmall2(cw, unicw, nc);
                memcpy(wspace,cw,(wl+1));
                rv = spellsharps(wspace, wspace, 0, 0, tmpword, info, root);
                if (!rv) {
                    wl2 = mkinitcap2(cw, unicw, nc);
                    rv = spellsharps(cw, cw, 0, 0, tmpword, info, root);
                }
                if ((abbv) && !(rv)) {
                    *(wspace+wl) = '.';
                    *(wspace+wl+1) = '\0';
                    rv = spellsharps(wspace, wspace, 0, 0, tmpword, info, root);
                    if (!rv) {
                        memcpy(wspace, cw, wl2);
                        *(wspace+wl2) = '.';
                        *(wspace+wl2+1) = '\0';
                        rv = spellsharps(wspace, wspace, 0, 0, tmpword, info, root);
                    }
                }
                if (rv) break;
            }
        }
     case INITCAP: { 
             wl = mkallsmall2(cw, unicw, nc);
             memcpy(wspace,cw,(wl+1));
             wl2 = mkinitcap2(cw, unicw, nc);
    	     if (captype == INITCAP) *info += SPELL_INITCAP;
             rv = checkword(cw, info, root);
    	     if (captype == INITCAP) *info -= SPELL_INITCAP;
             // forbid bad capitalization
             // (for example, ijs -> Ijs instead of IJs in Dutch)
             // use explicit forms in dic: Ijs/F (F = FORBIDDENWORD flag)
             if (*info & SPELL_FORBIDDEN) {
                rv = NULL;
                break;
             }             
             if (rv && is_keepcase(rv) && (captype == ALLCAP)) rv = NULL;
             if (rv) break;

             rv = checkword(wspace, info, root);
             if (abbv && !rv) {

                 *(wspace+wl) = '.';
                 *(wspace+wl+1) = '\0';
                 rv = checkword(wspace, info, root);
                 if (!rv) {
                    memcpy(wspace, cw, wl2);
                    *(wspace+wl2) = '.';
                    *(wspace+wl2+1) = '\0';
    	    	    if (captype == INITCAP) *info += SPELL_INITCAP;
                    rv = checkword(wspace, info, root);
    	    	    if (captype == INITCAP) *info -= SPELL_INITCAP;
                    if (rv && is_keepcase(rv) && (captype == ALLCAP)) rv = NULL;
                    break;
                 }
             }
             if (rv && is_keepcase(rv) &&
                ((captype == ALLCAP) ||
                   // if CHECKSHARPS: KEEPCASE words with \xDF  are allowed
                   // in INITCAP form, too.
                   !(pAMgr->get_checksharps() &&
                      ((utf8 && strstr(wspace, "\xC3\x9F")) ||
                      (!utf8 && strchr(wspace, '\xDF')))))) rv = NULL;             
             break;
           }               
  }
  
  if (rv) return 1;

  // recursive breaking at break points (not good for morphological analysis)
  if (wordbreak) {
    char * s;
    char r;
    int corr = 0;
    // German words beginning with "-" are not accepted
    if (langnum == LANG_de) corr = 1;
    int numbreak = pAMgr ? pAMgr->get_numbreak() : 0;
    for (int j = 0; j < numbreak; j++) {
      s=(char *) strstr(cw + corr, wordbreak[j]);
      if (s) {
        r = *s;
        *s = '\0';
        // examine 2 sides of the break point
        if (spell(cw) && spell(s + strlen(wordbreak[j]))) {
            *s = r;
            return 1;
        }
        *s = r;
      }
    }
  }

  // LANG_hu: compoundings with dashes and n-dashes XXX deprecated!
  if (langnum == LANG_hu) {
    int n;
    // compound word with dash (HU) I18n
    char * dash;
    int result = 0;
    // n-dash
    dash = (char *) strstr(cw,"\xE2\x80\x93");
    if (dash && !wordbreak) {
        *dash = '\0';
        // examine 2 sides of the dash
        if (spell(cw) && spell(dash + 3)) {
            *dash = '\xE2';
            return 1;
        }
        *dash = '\xE2';
    }
    dash = (char *) strchr(cw,'-');
    if (dash) {
        *dash='\0';      
        // examine 2 sides of the dash
        if (dash[1] == '\0') { // base word ending with dash
            if (spell(cw)) return 1;
        } else {
            // first word ending with dash: word-
            char r2 = *(dash + 1);
            dash[0]='-';
            dash[1]='\0';
            result = spell(cw);
            dash[1] = r2;
            dash[0]='\0';
            if (result && spell(dash+1) && ((strlen(dash+1) > 1) || (dash[1] == 'e') ||
                ((dash[1] > '0') && (dash[1] < '9')))) return 1;
        }
        // affixed number in correct word
        if (result && (dash > cw) && (((*(dash-1)<='9') && (*(dash-1)>='0')) || (*(dash-1)>='.'))) {
            *dash='-';
            n = 1;
            if (*(dash - n) == '.') n++;
            // search first not a number character to left from dash
            while (((dash - n)>=cw) && ((*(dash - n)=='0') || (n < 3)) && (n < 6)) {
                n++;
            }
            if ((dash - n) < cw) n--;
            // numbers: deprecated
            for(; n >= 1; n--) {
                if ((*(dash - n) >= '0') && (*(dash - n) <= '9') &&
                    checkword(dash - n, info, root)) return 1;
            }
        }
    }
  }
  return 0;
}

struct hentry * Hunspell::checkword(const char * w, int * info, char ** root)
{
  struct hentry * he = NULL;
  int len;
  char w2[MAXWORDUTF8LEN];
  const char * word;

  char * ignoredchars = pAMgr->get_ignore();
  if (ignoredchars != NULL) {
     strcpy(w2, w);
     if (utf8) {
        int ignoredchars_utf16_len;
        unsigned short * ignoredchars_utf16 = pAMgr->get_ignore_utf16(&ignoredchars_utf16_len);
        remove_ignored_chars_utf(w2, ignoredchars_utf16, ignoredchars_utf16_len);
     } else {
        remove_ignored_chars(w2,ignoredchars);
     }
     word = w2;
  } else word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    if (word != w2) {
      strcpy(w2, word);
      word = w2;
    }
    if (utf8) reverseword_utf(w2); else reverseword(w2);
  }

  // look word in hash table
  if (pHMgr) he = pHMgr->lookup(word);

  // check forbidden and onlyincompound words
  if ((he) && (he->astr) && (pAMgr) && TESTAFF(he->astr, pAMgr->get_forbiddenword(), he->alen)) {
    if (info) *info += SPELL_FORBIDDEN;
    // LANG_hu section: set dash information for suggestions
    if (langnum == LANG_hu) {
        if (pAMgr->get_compoundflag() &&
            TESTAFF(he->astr, pAMgr->get_compoundflag(), he->alen)) {
                if (info) *info += SPELL_COMPOUND;
        }
    }
    return NULL;
  }

  // he = next not pseudoroot, onlyincompound homonym or onlyupcase word
  while (he && (he->astr) &&
    ((pAMgr->get_pseudoroot() && TESTAFF(he->astr, pAMgr->get_pseudoroot(), he->alen)) ||
       (pAMgr->get_onlyincompound() && TESTAFF(he->astr, pAMgr->get_onlyincompound(), he->alen)) ||
       (info && (*info & SPELL_INITCAP) && TESTAFF(he->astr, ONLYUPCASEFLAG, he->alen))
    )) he = he->next_homonym;

  // check with affixes
  if (!he && pAMgr) {
     // try stripping off affixes */
     len = strlen(word);
     he = pAMgr->affix_check(word, len, 0);

     // check compound restriction and onlyupcase
     if (he && he->astr && (
        (pAMgr->get_onlyincompound() && 
    	    TESTAFF(he->astr, pAMgr->get_onlyincompound(), he->alen)) || 
        (info && (*info & SPELL_INITCAP) &&
    	    TESTAFF(he->astr, ONLYUPCASEFLAG, he->alen)))) {
    	    he = NULL;
     }

     if (he) {
        if ((he->astr) && (pAMgr) && TESTAFF(he->astr, pAMgr->get_forbiddenword(), he->alen)) {
            if (info) *info += SPELL_FORBIDDEN;
            return NULL;
        }
        if (root) {
            *root = mystrdup(&(he->word));
            if (complexprefixes) {
                if (utf8) reverseword_utf(*root); else reverseword(*root);
            }
        }
     // try check compound word
     } else if (pAMgr->get_compound()) {
          he = pAMgr->compound_check(word, len, 
                                  0,0,100,0,NULL,0,NULL,NULL,0);
          // LANG_hu section: `moving rule' with last dash
          if ((!he) && (langnum == LANG_hu) && (word[len-1]=='-')) {
             char * dup = mystrdup(word);
             dup[len-1] = '\0';
             he = pAMgr->compound_check(dup, len-1, 
                                  -5,0,100,0,NULL,1,NULL,NULL,0);
             free(dup);
          }
          // end of LANG speficic region          
          if (he) {
                if (root) {
                    *root = mystrdup(&(he->word));
                    if (complexprefixes) {
                        if (utf8) reverseword_utf(*root); else reverseword(*root);
                    }
                }
                if (info) *info += SPELL_COMPOUND;
          }
     }

  }

  return he;
}

int Hunspell::suggest(char*** slst, const char * word)
{
  int onlycmpdsug = 0;
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  w_char unicw[MAXWORDLEN];
  int nc = strlen(word);
  if (utf8) {
    if (nc >= MAXWORDUTF8LEN) return 0;
  } else {
    if (nc >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  int wl = cleanword2(cw, word, unicw, &nc, &captype, &abbv);
  if (wl == 0) return 0;
  int ns = 0;
  *slst = NULL;
  int capwords = 0;

  switch(captype) {
     case NOCAP:   { 
                     ns = pSMgr->suggest(slst, cw, ns, &onlycmpdsug);
                     break;
                   }

     case INITCAP: { 
                     capwords = 1;
                     ns = pSMgr->suggest(slst, cw, ns, &onlycmpdsug);
                     if (ns == -1) break;
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall2(wspace, unicw, nc);
                     ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                     break;
                   }
     case HUHINITCAP:
                    capwords = 1;
     case HUHCAP: { 
                     ns = pSMgr->suggest(slst, cw, ns, &onlycmpdsug);
                     if (ns != -1) {
                        int prevns;
    		        // something.The -> something. The
                        char * dot = strchr(cw, '.');
		        if (dot && (dot > cw)) {
		            int captype_;
		            if (utf8) {
		               w_char w_[MAXWORDLEN];
			       int wl_ = u8_u16(w_, MAXWORDLEN, dot + 1);
		               captype_ = get_captype_utf8(w_, wl_, langnum);
		            } else captype_ = get_captype(dot+1, strlen(dot+1), csconv);
		    	    if (captype_ == INITCAP) {
                        	char * st = mystrdup(cw);
                        	st = (char *) realloc(st, wl + 2);
				if (st) {
                        		st[(dot - cw) + 1] = ' ';
                        		strcpy(st + (dot - cw) + 2, dot + 1);
                    			ns = insert_sug(slst, st, ns);
					free(st);
				}
		    	    }
		        }
                        if (captype == HUHINITCAP) {
                            // TheOpenOffice.org -> The OpenOffice.org
                            memcpy(wspace,cw,(wl+1));
                            mkinitsmall2(wspace, unicw, nc);
                            ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                        }
                        memcpy(wspace,cw,(wl+1));
                        mkallsmall2(wspace, unicw, nc);
                        if (spell(wspace)) ns = insert_sug(slst, wspace, ns);
                        prevns = ns;
                        ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                        if (captype == HUHINITCAP) {
                            mkinitcap2(wspace, unicw, nc);
                            if (spell(wspace)) ns = insert_sug(slst, wspace, ns);
                            ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                        }
                        // aNew -> "a New" (instead of "a new")
                        for (int j = prevns; j < ns; j++) {
                           char * space = strchr((*slst)[j],' ');
                           if (space) {
                                int slen = strlen(space + 1);
                                // different case after space (need capitalisation)
                                if ((slen < wl) && strcmp(cw + wl - slen, space + 1)) {
                                    w_char w[MAXWORDLEN];
                                    int wc = 0;
                                    char * r = (*slst)[j];
                                    if (utf8) wc = u8_u16(w, MAXWORDLEN, space + 1);
                                    mkinitcap2(space + 1, w, wc);
                                    // set as first suggestion
                                    for (int k = j; k > 0; k--) (*slst)[k] = (*slst)[k - 1];
                                    (*slst)[0] = r;
                                }
                           }
                        }
                     }
                     break;
                   }

     case ALLCAP: { 
                     memcpy(wspace, cw, (wl+1));
                     mkallsmall2(wspace, unicw, nc);
                     ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                     if (ns == -1) break;
                     if (pAMgr && pAMgr->get_keepcase() && spell(wspace))
                        ns = insert_sug(slst, wspace, ns);
                     mkinitcap2(wspace, unicw, nc);
                     ns = pSMgr->suggest(slst, wspace, ns, &onlycmpdsug);
                     for (int j=0; j < ns; j++) {
                        mkallcap((*slst)[j]);
                        if (pAMgr && pAMgr->get_checksharps()) {
                            char * pos;
                            if (utf8) {
                                pos = strstr((*slst)[j], "\xC3\x9F");
                                while (pos) {
                                    *pos = 'S';
                                    *(pos+1) = 'S';
                                    pos = strstr(pos+2, "\xC3\x9F");
                                }
                            } else {
                                pos = strchr((*slst)[j], '\xDF');
                                while (pos) {
                                    (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 2);
                                    mystrrep((*slst)[j], "\xDF", "SS");
                                    pos = strchr((*slst)[j], '\xDF');
                                }
                            }
                        }
                     }
                     break;
                   }
  }

  // LANG_hu section: replace '-' with ' ' in Hungarian
  if (langnum == LANG_hu) {
      for (int j=0; j < ns; j++) {
          char * pos = strchr((*slst)[j],'-');
          if (pos) {
              int info;
              char w[MAXWORDUTF8LEN];
              *pos = '\0';
              strcpy(w, (*slst)[j]);
              strcat(w, pos + 1);
              spell(w, &info, NULL);
              if ((info & SPELL_COMPOUND) && (info & SPELL_FORBIDDEN)) {
                  *pos = ' ';
              } else *pos = '-';
          }
      }
  }
  // END OF LANG_hu section
  
  // try ngram approach since found nothing
  if ((ns == 0 || onlycmpdsug) && pAMgr && (pAMgr->get_maxngramsugs() != 0)) {
      switch(captype) {
          case NOCAP: {
              ns = pSMgr->ngsuggest(*slst, cw, ns, pHMgr);
              break;
          }
          case HUHCAP: {
              memcpy(wspace,cw,(wl+1));
              mkallsmall2(wspace, unicw, nc);
              ns = pSMgr->ngsuggest(*slst, wspace, ns, pHMgr);
              break;
          }
          case INITCAP: { 
              capwords = 1;
              memcpy(wspace,cw,(wl+1));
              mkallsmall2(wspace, unicw, nc);
              ns = pSMgr->ngsuggest(*slst, wspace, ns, pHMgr);
              break;
          }
          case ALLCAP: {
              memcpy(wspace,cw,(wl+1));
              mkallsmall2(wspace, unicw, nc);
	      int oldns = ns;
              ns = pSMgr->ngsuggest(*slst, wspace, ns, pHMgr);
              for (int j = oldns; j < ns; j++) 
                  mkallcap((*slst)[j]);
              break;
         }
      }
  }

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    for (int j = 0; j < ns; j++) {
      if (utf8) reverseword_utf((*slst)[j]); else reverseword((*slst)[j]);
    }
  }

  // capitalize
  if (capwords) for (int j=0; j < ns; j++) {
      mkinitcap((*slst)[j]);
  }

  // expand suggestions with dot(s)
  if (abbv && pAMgr && pAMgr->get_sugswithdots()) {
    for (int j = 0; j < ns; j++) {
      (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 1 + abbv);
      strcat((*slst)[j], word + strlen(word) - abbv);
    }
  }

  // remove bad capitalized and forbidden forms
  if (pAMgr && (pAMgr->get_keepcase() || pAMgr->get_forbiddenword())) {
  switch (captype) {
    case INITCAP:
    case ALLCAP: {
      int l = 0;
      for (int j=0; j < ns; j++) {
        if (!strchr((*slst)[j],' ') && !spell((*slst)[j])) {
          char s[MAXSWUTF8L];
          w_char w[MAXSWL];
          int len;
          if (utf8) {
            len = u8_u16(w, MAXSWL, (*slst)[j]);
          } else {
            strcpy(s, (*slst)[j]);
            len = strlen(s);
          }
          mkallsmall2(s, w, len);
          free((*slst)[j]);          
          if (spell(s)) {
            (*slst)[l] = mystrdup(s);
            l++;
          } else {
            mkinitcap2(s, w, len);
            if (spell(s)) {
              (*slst)[l] = mystrdup(s);
              l++;
            }
          }
        } else {
          (*slst)[l] = (*slst)[j];
          l++;
        }    
      }
      ns = l;
    }
  }
  }

  // remove duplications
  int l = 0;
  for (int j = 0; j < ns; j++) {
    (*slst)[l] = (*slst)[j];
    for (int k = 0; k < l; k++) {
      if (strcmp((*slst)[k], (*slst)[j]) == 0) {
        free((*slst)[j]);
        l--;
      }
    }
    l++;
  }
  return l;
}

char * Hunspell::get_dic_encoding()
{
  return encoding;
}

#ifdef HUNSPELL_EXPERIMENTAL
// XXX need UTF-8 support
int Hunspell::suggest_auto(char*** slst, const char * word)
{
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (utf8) {
    if (wl >= MAXWORDUTF8LEN) return 0;
  } else {
    if (wl >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 0;
  int ns = 0;
  *slst = NULL; // HU, nsug in pSMgr->suggest
  
  switch(captype) {
     case NOCAP:   { 
                     ns = pSMgr->suggest_auto(slst, cw, ns);
                     if (ns>0) break;
                     break;
                   }

     case INITCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     ns = pSMgr->suggest_auto(slst, wspace, ns);
                     for (int j=0; j < ns; j++)
                       mkinitcap((*slst)[j]);
                     ns = pSMgr->suggest_auto(slst, cw, ns);
                     break;
                     
                   }

     case HUHCAP: { 
                     ns = pSMgr->suggest_auto(slst, cw, ns);
                     if (ns == 0) {
                        memcpy(wspace,cw,(wl+1));
                        mkallsmall(wspace);
                        ns = pSMgr->suggest_auto(slst, wspace, ns);
                     }
                     break;
                   }

     case ALLCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     ns = pSMgr->suggest_auto(slst, wspace, ns);

                     mkinitcap(wspace);
                     ns = pSMgr->suggest_auto(slst, wspace, ns);

                     for (int j=0; j < ns; j++)
                       mkallcap((*slst)[j]);
                     break;
                   }
  }

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    for (int j = 0; j < ns; j++) {
      if (utf8) reverseword_utf((*slst)[j]); else reverseword((*slst)[j]);
    }
  }

  // expand suggestions with dot(s)
  if (abbv && pAMgr && pAMgr->get_sugswithdots()) {
    for (int j = 0; j < ns; j++) {
      (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 1 + abbv);
      strcat((*slst)[j], word + strlen(word) - abbv);
    }
  }

  // LANG_hu section: replace '-' with ' ' in Hungarian
  if (langnum == LANG_hu) {
      for (int j=0; j < ns; j++) {
          char * pos = strchr((*slst)[j],'-');
          if (pos) {
              int info;
              char w[MAXWORDUTF8LEN];
              *pos = '\0';
              strcpy(w, (*slst)[j]);
              strcat(w, pos + 1);
              spell(w, &info, NULL);
              if ((info & SPELL_COMPOUND) && (info & SPELL_FORBIDDEN)) {
                  *pos = ' ';
              } else *pos = '-';
          }
      }
  }
  // END OF LANG_hu section  
  return ns;
}

// XXX need UTF-8 support
int Hunspell::stem(char*** slst, const char * word)
{
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (utf8) {
    if (wl >= MAXWORDUTF8LEN) return 0;
  } else {
    if (wl >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 0;
  
  int ns = 0;

  *slst = NULL; // HU, nsug in pSMgr->suggest
  
  switch(captype) {
     case HUHCAP:
     case NOCAP:   { 
                     ns = pSMgr->suggest_stems(slst, cw, ns);

                     if ((abbv) && (ns == 0)) {
                         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         ns = pSMgr->suggest_stems(slst, wspace, ns);
                     }

                     break;
                   }

     case INITCAP: { 

                     ns = pSMgr->suggest_stems(slst, cw, ns);

                     if (ns == 0) {
                        memcpy(wspace,cw,(wl+1));
                        mkallsmall(wspace);
                        ns = pSMgr->suggest_stems(slst, wspace, ns);

                     }

                     if ((abbv) && (ns == 0)) {
                         memcpy(wspace,cw,wl);
                         mkallsmall(wspace);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         ns = pSMgr->suggest_stems(slst, wspace, ns);
                     }
                     
                     break;
                     
                   }

     case ALLCAP: { 
                     ns = pSMgr->suggest_stems(slst, cw, ns);
                     if (ns != 0) break;
                     
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     ns = pSMgr->suggest_stems(slst, wspace, ns);

                     if (ns == 0) {
                         mkinitcap(wspace);
                         ns = pSMgr->suggest_stems(slst, wspace, ns);
                     }

                     if ((abbv) && (ns == 0)) {
                         memcpy(wspace,cw,wl);
                         mkallsmall(wspace);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         ns = pSMgr->suggest_stems(slst, wspace, ns);
                     }


                     break;
                   }
  }
  
  return ns;
}

int Hunspell::suggest_pos_stems(char*** slst, const char * word)
{
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (utf8) {
    if (wl >= MAXWORDUTF8LEN) return 0;
  } else {
    if (wl >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 0;
  
  int ns = 0; // ns=0 = normalized input

  *slst = NULL; // HU, nsug in pSMgr->suggest
  
  switch(captype) {
     case HUHCAP:
     case NOCAP:   { 
                     ns = pSMgr->suggest_pos_stems(slst, cw, ns);

                     if ((abbv) && (ns == 0)) {
                         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
                     }

                     break;
                   }

     case INITCAP: { 

                     ns = pSMgr->suggest_pos_stems(slst, cw, ns);

                     if (ns == 0 || ((*slst)[0][0] == '#')) {
                        memcpy(wspace,cw,(wl+1));
                        mkallsmall(wspace);
                        ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
                     }
                     
                     break;
                     
                   }

     case ALLCAP: { 
                     ns = pSMgr->suggest_pos_stems(slst, cw, ns);
                     if (ns != 0) break;
                     
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     ns = pSMgr->suggest_pos_stems(slst, wspace, ns);

                     if (ns == 0) {
                         mkinitcap(wspace);
                         ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
                     }
                     break;
                   }
  }

  return ns;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE

const char * Hunspell::get_wordchars()
{
  return pAMgr->get_wordchars();
}

unsigned short * Hunspell::get_wordchars_utf16(int * len)
{
  return pAMgr->get_wordchars_utf16(len);
}

void Hunspell::mkinitcap(char * p)
{
  if (!utf8) {
    if (*p != '\0') *p = csconv[((unsigned char)*p)].cupper;
  } else {
      int len;
      w_char u[MAXWORDLEN];
      len = u8_u16(u, MAXWORDLEN, p);
      unsigned short i = unicodetoupper((u[0].h << 8) + u[0].l, langnum);
      u[0].h = (unsigned char) (i >> 8);
      u[0].l = (unsigned char) (i & 0x00FF);
      u16_u8(p, MAXWORDUTF8LEN, u, len);
  }
}

int Hunspell::mkinitcap2(char * p, w_char * u, int nc)
{
  if (!utf8) {
    if (*p != '\0') *p = csconv[((unsigned char)*p)].cupper;
  } else if (nc > 0) {
      unsigned short i = unicodetoupper((u[0].h << 8) + u[0].l, langnum);
      u[0].h = (unsigned char) (i >> 8);
      u[0].l = (unsigned char) (i & 0x00FF);
      u16_u8(p, MAXWORDUTF8LEN, u, nc);
      return strlen(p);
  }
  return nc;
}

int Hunspell::mkinitsmall2(char * p, w_char * u, int nc)
{
  if (!utf8) {
    if (*p != '\0') *p = csconv[((unsigned char)*p)].clower;
  } else if (nc > 0) {
      unsigned short i = unicodetolower((u[0].h << 8) + u[0].l, langnum);
      u[0].h = (unsigned char) (i >> 8);
      u[0].l = (unsigned char) (i & 0x00FF);
      u16_u8(p, MAXWORDUTF8LEN, u, nc);
      return strlen(p);
  }
  return nc;
}

int Hunspell::put_word(const char * word)
{
    if (pHMgr) return pHMgr->put_word(word, NULL);
    return 0;
}

int Hunspell::put_word_pattern(const char * word, const char * pattern)
{
    if (pHMgr) return pHMgr->put_word_pattern(word, pattern);
    return 0;
}

const char * Hunspell::get_version()
{
  return pAMgr->get_version();
}

struct cs_info * Hunspell::get_csconv()
{
  return csconv;
}

#ifdef HUNSPELL_EXPERIMENTAL
// XXX need UTF-8 support
char * Hunspell::morph(const char * word)
{
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (utf8) {
    if (wl >= MAXWORDUTF8LEN) return 0;
  } else {
    if (wl >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) {
      if (abbv) {
          for (wl = 0; wl < abbv; wl++) cw[wl] = '.';
          cw[wl] = '\0';
          abbv = 0;
      } else return 0;
  }

  char result[MAXLNLEN];
  char * st = NULL;
  
  *result = '\0';

  int n = 0;
  int n2 = 0;
  int n3 = 0;

  // test numbers
  // LANG_hu section: set dash information for suggestions
  if (langnum == LANG_hu) {
  while ((n < wl) && 
        (((cw[n] <= '9') && (cw[n] >= '0')) || (((cw[n] == '.') || (cw[n] == ',')) && (n > 0)))) {
        n++;
        if ((cw[n] == '.') || (cw[n] == ',')) {
                if (((n2 == 0) && (n > 3)) || 
                        ((n2 > 0) && ((cw[n-1] == '.') || (cw[n-1] == ',')))) break;
                n2++;
                n3 = n;
        }
  }

  if ((n == wl) && (n3 > 0) && (n - n3 > 3)) return NULL;
  if ((n == wl) || ((n>0) && ((cw[n]=='%') || (cw[n]=='\xB0')) && checkword(cw+n, NULL, NULL))) {
        strcat(result, cw);
        result[n - 1] = '\0';
        if (n == wl) {
                st = pSMgr->suggest_morph(cw + n - 1);
                if (st) {
                        strcat(result, st);
                        free(st);
                }
        } else {
                char sign = cw[n];
                cw[n] = '\0';
                st = pSMgr->suggest_morph(cw + n - 1);
                if (st) {
                        strcat(result, st);
                        free(st);
                }
                strcat(result, "+"); // XXX SPEC. MORPHCODE
                cw[n] = sign;
                st = pSMgr->suggest_morph(cw + n);
                if (st) {
                        strcat(result, st);
                        free(st);
                }
        }
        return mystrdup(result);
  }
  }
  // END OF LANG_hu section
  
  switch(captype) {
     case NOCAP:   { 
                     st = pSMgr->suggest_morph(cw);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                                        memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         st = pSMgr->suggest_morph(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                     }
                                         break;
                   }
     case INITCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph(wspace);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }                                   
                         st = pSMgr->suggest_morph(cw);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                                         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         mkallsmall(wspace);
                         st = pSMgr->suggest_morph(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                         mkinitcap(wspace);
                         st = pSMgr->suggest_morph(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                     }
                     break;
                   }
     case HUHCAP: { 
                     st = pSMgr->suggest_morph(cw);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }
#if 0
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
#endif
                     break;
                 }
     case ALLCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     st = pSMgr->suggest_morph(wspace);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }               
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                             mkinitcap(wspace);
                             st = pSMgr->suggest_morph(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                        memcpy(wspace,cw,(wl+1));
                        *(wspace+wl) = '.';
                        *(wspace+wl+1) = '\0';
                        if (*result) strcat(result, "\n");
                        st = pSMgr->suggest_morph(wspace);
                        if (st) {
                                strcat(result, st);
                                free(st);
                        }                    
                        mkallsmall(wspace);
                        st = pSMgr->suggest_morph(wspace);
                        if (st) {
                          if (*result) strcat(result, "\n");
                          strcat(result, st);
                          free(st);
                        }
                                mkinitcap(wspace);
                                st = pSMgr->suggest_morph(wspace);
                        if (st) {
                          if (*result) strcat(result, "\n");
                          strcat(result, st);
                          free(st);
                        }
                                         }
                     break;
                   }
  }

  if (result && (*result)) {
    // word reversing wrapper for complex prefixes
    if (complexprefixes) {
      if (utf8) reverseword_utf(result); else reverseword(result);
    }
    return mystrdup(result);
  }

  // compound word with dash (HU) I18n
  char * dash = NULL;
  int nresult = 0;
  // LANG_hu section: set dash information for suggestions
  if (langnum == LANG_hu) dash = (char *) strchr(cw,'-');
  if ((langnum == LANG_hu) && dash) {
      *dash='\0';      
      // examine 2 sides of the dash
      if (dash[1] == '\0') { // base word ending with dash
        if (spell(cw)) return pSMgr->suggest_morph(cw);
      } else if ((dash[1] == 'e') && (dash[2] == '\0')) { // XXX (HU) -e hat.
        if (spell(cw) && (spell("-e"))) {
                        st = pSMgr->suggest_morph(cw);
                        if (st) {
                                strcat(result, st);
                                free(st);
                        }
                        strcat(result,"+"); // XXX spec. separator in MORPHCODE
                        st = pSMgr->suggest_morph("-e");
                        if (st) {
                                strcat(result, st);
                                free(st);
                        }
                        return mystrdup(result);
                }
      } else {
      // first word ending with dash: word- XXX ???
        char r2 = *(dash + 1);
        dash[0]='-';
        dash[1]='\0';
        nresult = spell(cw);
        dash[1] = r2;
        dash[0]='\0';
        if (nresult && spell(dash+1) && ((strlen(dash+1) > 1) ||
                ((dash[1] > '0') && (dash[1] < '9')))) {
                            st = morph(cw);
                            if (st) {
                                strcat(result, st);
                                    free(st);
                                strcat(result,"+"); // XXX spec. separator in MORPHCODE
                            }
                            st = morph(dash+1);
                            if (st) {
                                    strcat(result, st);
                                    free(st);
                            }
                            return mystrdup(result);                    
                        }
      }
      // affixed number in correct word
     if (nresult && (dash > cw) && (((*(dash-1)<='9') && 
                        (*(dash-1)>='0')) || (*(dash-1)=='.'))) {
         *dash='-';
         n = 1;
         if (*(dash - n) == '.') n++;
         // search first not a number character to left from dash
         while (((dash - n)>=cw) && ((*(dash - n)=='0') || (n < 3)) && (n < 6)) {
            n++;
         }
         if ((dash - n) < cw) n--;
         // numbers: valami1000000-hoz
         // examine 100000-hoz, 10000-hoz 1000-hoz, 10-hoz,
         // 56-hoz, 6-hoz
         for(; n >= 1; n--) {
            if ((*(dash - n) >= '0') && (*(dash - n) <= '9') && checkword(dash - n, NULL, NULL)) {
                    strcat(result, cw);
                    result[dash - cw - n] = '\0';
                        st = pSMgr->suggest_morph(dash - n);
                        if (st) {
                        strcat(result, st);
                                free(st);
                        }
                    return mystrdup(result);                    
            }
         }
     }
  }
  return NULL;
}

// XXX need UTF-8 support
char * Hunspell::morph_with_correction(const char * word)
{
  char cw[MAXWORDUTF8LEN];
  char wspace[MAXWORDUTF8LEN];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (utf8) {
    if (wl >= MAXWORDUTF8LEN) return 0;
  } else {
    if (wl >= MAXWORDLEN) return 0;
  }
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 0;

  char result[MAXLNLEN];
  char * st = NULL;
  
  *result = '\0';
  
  
  switch(captype) {
     case NOCAP:   { 
                     st = pSMgr->suggest_morph_for_spelling_error(cw);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                                        memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         st = pSMgr->suggest_morph_for_spelling_error(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                     }
                                         break;
                   }
     case INITCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph_for_spelling_error(wspace);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }                                   
                         st = pSMgr->suggest_morph_for_spelling_error(cw);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                                         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         mkallsmall(wspace);
                         st = pSMgr->suggest_morph_for_spelling_error(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                         mkinitcap(wspace);
                         st = pSMgr->suggest_morph_for_spelling_error(wspace);
                         if (st) {
                            if (*result) strcat(result, "\n");
                            strcat(result, st);
                            free(st);
                                                 }
                     }
                     break;
                   }
     case HUHCAP: { 
                     st = pSMgr->suggest_morph_for_spelling_error(cw);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph_for_spelling_error(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }               
                     break;
                 }
     case ALLCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     st = pSMgr->suggest_morph_for_spelling_error(wspace);
                     if (st) {
                        strcat(result, st);
                        free(st);
                     }               
                     mkallsmall(wspace);
                     st = pSMgr->suggest_morph_for_spelling_error(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                             mkinitcap(wspace);
                             st = pSMgr->suggest_morph_for_spelling_error(wspace);
                     if (st) {
                        if (*result) strcat(result, "\n");
                        strcat(result, st);
                        free(st);
                     }
                                         if (abbv) {
                        memcpy(wspace,cw,(wl+1));
                        *(wspace+wl) = '.';
                        *(wspace+wl+1) = '\0';
                        if (*result) strcat(result, "\n");
                        st = pSMgr->suggest_morph_for_spelling_error(wspace);
                        if (st) {
                                strcat(result, st);
                                free(st);
                        }                    
                        mkallsmall(wspace);
                        st = pSMgr->suggest_morph_for_spelling_error(wspace);
                        if (st) {
                          if (*result) strcat(result, "\n");
                          strcat(result, st);
                          free(st);
                        }
                                mkinitcap(wspace);
                                st = pSMgr->suggest_morph_for_spelling_error(wspace);
                        if (st) {
                          if (*result) strcat(result, "\n");
                          strcat(result, st);
                          free(st);
                        }
                                         }
                     break;
                   }
  }

  if (result) return mystrdup(result);
  return NULL;
}

/* analyze word
 * return line count 
 * XXX need a better data structure for morphological analysis */
int Hunspell::analyze(char ***out, const char *word) {
  int  n = 0;
  if (!word) return 0;
  char * m = morph(word);
  if(!m) return 0;
  if (!out)
  {
     n = line_tok(m, out);
     free(m);
     return n;
  }

  // without memory allocation
  /* BUG missing buffer size checking */
  int i, p;
  for(p = 0, i = 0; m[i]; i++) {
     if(m[i] == '\n' || !m[i+1]) {
       n++;
       strncpy((*out)[n++], m + p, i - p + 1);
       if (m[i] == '\n') (*out)[n++][i - p] = '\0';
       if(!m[i+1]) break;
       p = i + 1;        
     }
  }
  free(m);
  return n;
}

#endif // END OF HUNSPELL_EXPERIMENTAL CODE

Hunhandle *Hunspell_create(const char * affpath, const char * dpath)
{
        return (Hunhandle*)(new Hunspell(affpath, dpath));
}

void Hunspell_destroy(Hunhandle *pHunspell)
{
        delete (Hunspell*)(pHunspell);
}

int Hunspell_spell(Hunhandle *pHunspell, const char *word)
{
        return ((Hunspell*)pHunspell)->spell(word);
}

char *Hunspell_get_dic_encoding(Hunhandle *pHunspell)
{
        return ((Hunspell*)pHunspell)->get_dic_encoding();
}

int Hunspell_suggest(Hunhandle *pHunspell, char*** slst, const char * word)
{
        return ((Hunspell*)pHunspell)->suggest(slst, word);
}
