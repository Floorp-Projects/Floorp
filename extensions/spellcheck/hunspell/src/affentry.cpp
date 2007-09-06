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
#include <cctype>
#include <cstdio>
#else
#include <stdlib.h> 
#include <string.h>
#include <stdio.h> 
#include <ctype.h>
#endif

#include "affentry.hxx"
#include "csutil.hxx"

#ifndef MOZILLA_CLIENT
#ifndef W32
using namespace std;
#endif
#endif


PfxEntry::PfxEntry(AffixMgr* pmgr, affentry* dp)
{
  // register affix manager
  pmyMgr = pmgr;

  // set up its intial values
 
  aflag = dp->aflag;         // flag 
  strip = dp->strip;         // string to strip
  appnd = dp->appnd;         // string to append
  stripl = dp->stripl;       // length of strip string
  appndl = dp->appndl;       // length of append string
  numconds = dp->numconds;   // number of conditions to match
  opts = dp->opts;         // cross product flag
  // then copy over all of the conditions
  memcpy(&conds.base[0],&dp->conds.base[0],SETSIZE*sizeof(conds.base[0]));
  next = NULL;
  nextne = NULL;
  nexteq = NULL;
#ifdef HUNSPELL_EXPERIMENTAL
  morphcode = dp->morphcode;
#endif
  contclass = dp->contclass;
  contclasslen = dp->contclasslen;
}


PfxEntry::~PfxEntry()
{
    aflag = 0;
    if (appnd) free(appnd);
    if (strip) free(strip);
    pmyMgr = NULL;
    appnd = NULL;
    strip = NULL;
    if (opts & aeUTF8) {
        for (int i = 0; i < numconds; i++) {
            if (conds.utf8.wchars[i]) free(conds.utf8.wchars[i]);
        }
    }
#ifdef HUNSPELL_EXPERIMENTAL
    if (morphcode && !(opts & aeALIASM)) free(morphcode);
#endif
    if (contclass && !(opts & aeALIASF)) free(contclass);
}

// add prefix to this word assuming conditions hold
char * PfxEntry::add(const char * word, int len)
{
    char tword[MAXWORDUTF8LEN + 4];

    if ((len > stripl) && (len >= numconds) && test_condition(word) &&
       (!stripl || (strncmp(word, strip, stripl) == 0)) && 
       ((MAXWORDUTF8LEN + 4) > (len + appndl - stripl))) {
    /* we have a match so add prefix */
              char * pp = tword;
              if (appndl) {
                  strcpy(tword,appnd);
                  pp += appndl;
               }
               strcpy(pp, (word + stripl));
               return mystrdup(tword);
     }
     return NULL;    
}


inline int PfxEntry::test_condition(const char * st)
{
    int cond;
    unsigned char * cp = (unsigned char *)st;
    if (!(opts & aeUTF8)) { // 256-character codepage
        for (cond = 0;  cond < numconds;  cond++) {
            if ((conds.base[*cp++] & (1 << cond)) == 0) return 0;
        }
    } else { // UTF-8 encoding
      unsigned short wc;
      for (cond = 0;  cond < numconds;  cond++) {
        // a simple 7-bit ASCII character in UTF-8
        if ((*cp >> 7) == 0) {
            // also check limit (end of word)
            if ((!*cp) || ((conds.utf8.ascii[*cp++] & (1 << cond)) == 0)) return 0;
        // UTF-8 multibyte character
        } else {
            // not dot wildcard in rule
            if (!conds.utf8.all[cond]) {
                if (conds.utf8.neg[cond]) {
                    u8_u16((w_char *) &wc, 1, (char *) cp);
                    if (conds.utf8.wchars[cond] && 
                        flag_bsearch((unsigned short *)conds.utf8.wchars[cond],
                            wc, (short) conds.utf8.wlen[cond])) return 0;
                } else {
                    if (!conds.utf8.wchars[cond]) return 0;
                    u8_u16((w_char *) &wc, 1, (char *) cp);
                    if (!flag_bsearch((unsigned short *)conds.utf8.wchars[cond],
                         wc, (short)conds.utf8.wlen[cond])) return 0;
                }
            }
            // jump to next UTF-8 character
            for(cp++; (*cp & 0xc0) == 0x80; cp++);
        }
      }
    }
    return 1;
}


// check if this prefix entry matches 
struct hentry * PfxEntry::checkword(const char * word, int len, char in_compound, const FLAG needflag)
{
    int                 tmpl;   // length of tmpword
    struct hentry *     he;     // hash entry of root word or NULL
    char                tmpword[MAXWORDUTF8LEN + 4];

    // on entry prefix is 0 length or already matches the beginning of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

     tmpl = len - appndl;

     if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing prefix and adding
            // back any characters that would have been stripped

            if (stripl) strcpy (tmpword, strip);
            strcpy ((tmpword + stripl), (word + appndl));

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then check if resulting
            // root word in the dictionary

            if (test_condition(tmpword)) {
                tmpl += stripl;
                if ((he = pmyMgr->lookup(tmpword)) != NULL) {
                   do {
                      if (TESTAFF(he->astr, aflag, he->alen) &&
                        // forbid single prefixes with pseudoroot flag
                        ! TESTAFF(contclass, pmyMgr->get_pseudoroot(), contclasslen) &&
                        // needflag
                        ((!needflag) || TESTAFF(he->astr, needflag, he->alen) ||
                         (contclass && TESTAFF(contclass, needflag, contclasslen))))
                            return he;
                      he = he->next_homonym; // check homonyms
                   } while (he);
                }
                
                // prefix matched but no root word was found 
                // if aeXPRODUCT is allowed, try again but now 
                // ross checked combined with a suffix

                //if ((opts & aeXPRODUCT) && in_compound) {
                if ((opts & aeXPRODUCT)) {
                   he = pmyMgr->suffix_check(tmpword, tmpl, aeXPRODUCT, (AffEntry *)this, NULL, 
                        0, NULL, FLAG_NULL, needflag, in_compound);
                   if (he) return he;
                }
            }
     }
    return NULL;
}

// check if this prefix entry matches 
struct hentry * PfxEntry::check_twosfx(const char * word, int len,
    char in_compound, const FLAG needflag)
{
    int                 tmpl;   // length of tmpword
    struct hentry *     he;     // hash entry of root word or NULL
    char                tmpword[MAXWORDUTF8LEN + 4];

    // on entry prefix is 0 length or already matches the beginning of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

     tmpl = len - appndl;

     if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing prefix and adding
            // back any characters that would have been stripped

            if (stripl) strcpy (tmpword, strip);
            strcpy ((tmpword + stripl), (word + appndl));

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then check if resulting
            // root word in the dictionary

            if (test_condition(tmpword)) {
                tmpl += stripl;

                // prefix matched but no root word was found 
                // if aeXPRODUCT is allowed, try again but now 
                // cross checked combined with a suffix

                if ((opts & aeXPRODUCT) && (in_compound != IN_CPD_BEGIN)) {
                   he = pmyMgr->suffix_check_twosfx(tmpword, tmpl, aeXPRODUCT, (AffEntry *)this, needflag);
                   if (he) return he;
                }
            }
     }
    return NULL;
}

#ifdef HUNSPELL_EXPERIMENTAL
// check if this prefix entry matches 
char * PfxEntry::check_twosfx_morph(const char * word, int len,
         char in_compound, const FLAG needflag)
{
    int                 tmpl;   // length of tmpword
    char                tmpword[MAXWORDUTF8LEN + 4];

    // on entry prefix is 0 length or already matches the beginning of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

     tmpl = len - appndl;

     if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing prefix and adding
            // back any characters that would have been stripped

            if (stripl) strcpy (tmpword, strip);
            strcpy ((tmpword + stripl), (word + appndl));

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then check if resulting
            // root word in the dictionary

            if (test_condition(tmpword)) {
                tmpl += stripl;

                // prefix matched but no root word was found 
                // if aeXPRODUCT is allowed, try again but now 
                // ross checked combined with a suffix

                if ((opts & aeXPRODUCT) && (in_compound != IN_CPD_BEGIN)) {
                    return pmyMgr->suffix_check_twosfx_morph(tmpword, tmpl,
                             aeXPRODUCT, (AffEntry *)this, needflag);
                }
            }
     }
    return NULL;
}

// check if this prefix entry matches 
char * PfxEntry::check_morph(const char * word, int len, char in_compound, const FLAG needflag)
{
    int                 tmpl;   // length of tmpword
    struct hentry *     he;     // hash entry of root word or NULL
    char                tmpword[MAXWORDUTF8LEN + 4];
    char                result[MAXLNLEN];
    char * st;
    
    *result = '\0';

    // on entry prefix is 0 length or already matches the beginning of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

     tmpl = len - appndl;

     if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing prefix and adding
            // back any characters that would have been stripped

            if (stripl) strcpy (tmpword, strip);
            strcpy ((tmpword + stripl), (word + appndl));

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then check if resulting
            // root word in the dictionary

            if (test_condition(tmpword)) {
                tmpl += stripl;
                if ((he = pmyMgr->lookup(tmpword)) != NULL) {
                    do {
                      if (TESTAFF(he->astr, aflag, he->alen) &&
                        // forbid single prefixes with pseudoroot flag
                        ! TESTAFF(contclass, pmyMgr->get_pseudoroot(), contclasslen) &&
                        // needflag
                        ((!needflag) || TESTAFF(he->astr, needflag, he->alen) ||
                         (contclass && TESTAFF(contclass, needflag, contclasslen)))) {
                            if (morphcode) strcat(result, morphcode); else strcat(result,getKey());
                            if (he->description) {
                                if ((*(he->description)=='[')||(*(he->description)=='<')) strcat(result,he->word);
                                strcat(result,he->description);
                            }
                            strcat(result, "\n");
                      }
                      he = he->next_homonym;
                    } while (he);
                }

                // prefix matched but no root word was found 
                // if aeXPRODUCT is allowed, try again but now 
                // ross checked combined with a suffix

                if ((opts & aeXPRODUCT) && (in_compound != IN_CPD_BEGIN)) {
                   st = pmyMgr->suffix_check_morph(tmpword, tmpl, aeXPRODUCT, (AffEntry *)this, 
                     FLAG_NULL, needflag);
                   if (st) {
                        strcat(result, st);
                        free(st);
                   }
                }
            }
     }
     
    if (*result) return mystrdup(result);
    return NULL;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE

SfxEntry::SfxEntry(AffixMgr * pmgr, affentry* dp)
{
  // register affix manager
  pmyMgr = pmgr;

  // set up its intial values
  aflag = dp->aflag;         // char flag 
  strip = dp->strip;         // string to strip
  appnd = dp->appnd;         // string to append
  stripl = dp->stripl;       // length of strip string
  appndl = dp->appndl;       // length of append string
  numconds = dp->numconds;   // number of conditions to match
  opts = dp->opts;         // cross product flag

  // then copy over all of the conditions
  memcpy(&conds.base[0],&dp->conds.base[0],SETSIZE*sizeof(conds.base[0]));

  rappnd = myrevstrdup(appnd);

#ifdef HUNSPELL_EXPERIMENTAL
  morphcode = dp->morphcode;
#endif
  contclass = dp->contclass;
  contclasslen = dp->contclasslen;
}


SfxEntry::~SfxEntry()
{
    aflag = 0;
    if (appnd) free(appnd);
    if (rappnd) free(rappnd);
    if (strip) free(strip);
    pmyMgr = NULL;
    appnd = NULL;
    strip = NULL;    
    if (opts & aeUTF8) {
        for (int i = 0; i < numconds; i++) {
            if (conds.utf8.wchars[i]) free(conds.utf8.wchars[i]);  
        }
    }
#ifdef HUNSPELL_EXPERIMENTAL
    if (morphcode && !(opts & aeALIASM)) free(morphcode);
#endif
    if (contclass && !(opts & aeALIASF)) free(contclass);
}

// add suffix to this word assuming conditions hold
char * SfxEntry::add(const char * word, int len)
{
    char                tword[MAXWORDUTF8LEN + 4];

     /* make sure all conditions match */
     if ((len > stripl) && (len >= numconds) && test_condition(word + len, word) &&
        (!stripl || (strcmp(word + len - stripl, strip) == 0)) &&
        ((MAXWORDUTF8LEN + 4) > (len + appndl - stripl))) {
              /* we have a match so add suffix */
              strcpy(tword,word);
              if (appndl) {
                  strcpy(tword + len - stripl, appnd);
              } else {
                  *(tword + len - stripl) = '\0';
              }
              return mystrdup(tword);
     }
     return NULL;
}


inline int SfxEntry::test_condition(const char * st, const char * beg)
{
    int cond;
    unsigned char * cp = (unsigned char *) st;
    if (!(opts & aeUTF8)) { // 256-character codepage
        // Domolki affix algorithm
        for (cond = numconds;  --cond >= 0; ) {
            if ((conds.base[*--cp] & (1 << cond)) == 0) return 0;
        }
    } else { // UTF-8 encoding
      unsigned short wc;
      for (cond = numconds;  --cond >= 0; ) {
        // go to next character position and check limit
        if ((char *) --cp < beg) return 0;
        // a simple 7-bit ASCII character in UTF-8
        if ((*cp >> 7) == 0) {
            if ((conds.utf8.ascii[*cp] & (1 << cond)) == 0) return 0;
        // UTF-8 multibyte character
        } else {
            // go to first character of UTF-8 multibyte character
            for (; (*cp & 0xc0) == 0x80; cp--);
            // not dot wildcard in rule
            if (!conds.utf8.all[cond]) {
                if (conds.utf8.neg[cond]) {
                    u8_u16((w_char *) &wc, 1, (char *) cp);
                    if (conds.utf8.wchars[cond] && 
                        flag_bsearch((unsigned short *)conds.utf8.wchars[cond],
                            wc, (short) conds.utf8.wlen[cond])) return 0;
                } else {
                    if (!conds.utf8.wchars[cond]) return 0;
                    u8_u16((w_char *) &wc, 1, (char *) cp);
                    if (!flag_bsearch((unsigned short *)conds.utf8.wchars[cond],
                         wc, (short)conds.utf8.wlen[cond])) return 0;
                }
            }
        }
      }
    }
    return 1;
}



// see if this suffix is present in the word 
struct hentry * SfxEntry::checkword(const char * word, int len, int optflags,
    AffEntry* ppfx, char ** wlst, int maxSug, int * ns, const FLAG cclass, const FLAG needflag,
    const FLAG badflag)
{
    int                 tmpl;            // length of tmpword 
    struct hentry *     he;              // hash entry pointer
    unsigned char *     cp;
    char                tmpword[MAXWORDUTF8LEN + 4];
    PfxEntry* ep = (PfxEntry *) ppfx;

    // if this suffix is being cross checked with a prefix
    // but it does not support cross products skip it

    if (((optflags & aeXPRODUCT) != 0) && ((opts & aeXPRODUCT) == 0))
        return NULL;

    // upon entry suffix is 0 length or already matches the end of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

    tmpl = len - appndl;
    // the second condition is not enough for UTF-8 strings
    // it checked in test_condition()
    
    if ((tmpl > 0)  &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing suffix and adding
            // back any characters that would have been stripped or
            // or null terminating the shorter string

            strcpy (tmpword, word);
            cp = (unsigned char *)(tmpword + tmpl);
            if (stripl) {
                strcpy ((char *)cp, strip);
                tmpl += stripl;
                cp = (unsigned char *)(tmpword + tmpl);
            } else *cp = '\0';

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being            // tested

            // if all conditions are met then check if resulting
            // root word in the dictionary

            if (test_condition((char *) cp, (char *) tmpword)) {

#ifdef SZOSZABLYA_POSSIBLE_ROOTS
                fprintf(stdout,"%s %s %c\n", word, tmpword, aflag);
#endif
                if ((he = pmyMgr->lookup(tmpword)) != NULL) {
                    do {
                        // check conditional suffix (enabled by prefix)
                        if ((TESTAFF(he->astr, aflag, he->alen) || (ep && ep->getCont() &&
                                    TESTAFF(ep->getCont(), aflag, ep->getContLen()))) && 
                            (((optflags & aeXPRODUCT) == 0) || 
                            TESTAFF(he->astr, ep->getFlag(), he->alen) ||
                             // enabled by prefix
                            ((contclass) && TESTAFF(contclass, ep->getFlag(), contclasslen))
                            ) &&
                            // handle cont. class
                            ((!cclass) || 
                                ((contclass) && TESTAFF(contclass, cclass, contclasslen))
                            ) &&
                            // check only in compound homonyms (bad flags)
                            (!badflag || !TESTAFF(he->astr, badflag, he->alen)
                            ) &&                            
                            // handle required flag
                            ((!needflag) || 
                              (TESTAFF(he->astr, needflag, he->alen) ||
                              ((contclass) && TESTAFF(contclass, needflag, contclasslen)))
                            )
                        ) return he;
                        he = he->next_homonym; // check homonyms
                    } while (he);

                // obsolote stemming code (used only by the 
                // experimental SuffixMgr:suggest_pos_stems)
                // store resulting root in wlst
                } else if (wlst && (*ns < maxSug)) {
                    int cwrd = 1;
                    for (int k=0; k < *ns; k++) 
                        if (strcmp(tmpword, wlst[k]) == 0) cwrd = 0;
                    if (cwrd) {
                        wlst[*ns] = mystrdup(tmpword);
                        if (wlst[*ns] == NULL) {
                            for (int j=0; j<*ns; j++) free(wlst[j]);
                            *ns = -1;
                            return NULL;
                        }
                        (*ns)++;
                    }
                }
            }
    }
    return NULL;
}

// see if two-level suffix is present in the word 
struct hentry * SfxEntry::check_twosfx(const char * word, int len, int optflags,
    AffEntry* ppfx, const FLAG needflag)
{
    int                 tmpl;            // length of tmpword 
    struct hentry *     he;              // hash entry pointer
    unsigned char *     cp;
    char                tmpword[MAXWORDUTF8LEN + 4];
    PfxEntry* ep = (PfxEntry *) ppfx;


    // if this suffix is being cross checked with a prefix
    // but it does not support cross products skip it

    if ((optflags & aeXPRODUCT) != 0 &&  (opts & aeXPRODUCT) == 0)
        return NULL;

    // upon entry suffix is 0 length or already matches the end of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

    tmpl = len - appndl;

    if ((tmpl > 0)  &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing suffix and adding
            // back any characters that would have been stripped or
            // or null terminating the shorter string

            strcpy (tmpword, word);
            cp = (unsigned char *)(tmpword + tmpl);
            if (stripl) {
                strcpy ((char *)cp, strip);
                tmpl += stripl;
                cp = (unsigned char *)(tmpword + tmpl);
            } else *cp = '\0';

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then recall suffix_check

            if (test_condition((char *) cp, (char *) tmpword)) {
                if (ppfx) {
                    // handle conditional suffix
                    if ((contclass) && TESTAFF(contclass, ep->getFlag(), contclasslen)) 
                        he = pmyMgr->suffix_check(tmpword, tmpl, 0, NULL, NULL, 0, NULL, (FLAG) aflag, needflag);
                    else
                        he = pmyMgr->suffix_check(tmpword, tmpl, optflags, ppfx, NULL, 0, NULL, (FLAG) aflag, needflag);
                } else {
                    he = pmyMgr->suffix_check(tmpword, tmpl, 0, NULL, NULL, 0, NULL, (FLAG) aflag, needflag);
                }
                if (he) return he;
            }
    }
    return NULL;
}

#ifdef HUNSPELL_EXPERIMENTAL
// see if two-level suffix is present in the word 
char * SfxEntry::check_twosfx_morph(const char * word, int len, int optflags,
    AffEntry* ppfx, const FLAG needflag)
{
    int                 tmpl;            // length of tmpword 
    unsigned char *     cp;
    char                tmpword[MAXWORDUTF8LEN + 4];
    PfxEntry* ep = (PfxEntry *) ppfx;
    char * st;

    char result[MAXLNLEN];
    
    *result = '\0';

    // if this suffix is being cross checked with a prefix
    // but it does not support cross products skip it

    if ((optflags & aeXPRODUCT) != 0 &&  (opts & aeXPRODUCT) == 0)
        return NULL;

    // upon entry suffix is 0 length or already matches the end of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

    tmpl = len - appndl;

    if ((tmpl > 0)  &&  (tmpl + stripl >= numconds)) {

            // generate new root word by removing suffix and adding
            // back any characters that would have been stripped or
            // or null terminating the shorter string

            strcpy (tmpword, word);
            cp = (unsigned char *)(tmpword + tmpl);
            if (stripl) {
                strcpy ((char *)cp, strip);
                tmpl += stripl;
                cp = (unsigned char *)(tmpword + tmpl);
            } else *cp = '\0';

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

            // if all conditions are met then recall suffix_check

            if (test_condition((char *) cp, (char *) tmpword)) {
                if (ppfx) {
                    // handle conditional suffix
                    if ((contclass) && TESTAFF(contclass, ep->getFlag(), contclasslen)) {
                        st = pmyMgr->suffix_check_morph(tmpword, tmpl, 0, NULL, aflag, needflag);
                        if (st) {
                            if (((PfxEntry *) ppfx)->getMorph()) {
                                strcat(result, ((PfxEntry *) ppfx)->getMorph());
                            }
                            strcat(result,st);
                            free(st);
                            mychomp(result);
                        }
                    } else {
                        st = pmyMgr->suffix_check_morph(tmpword, tmpl, optflags, ppfx, aflag, needflag);
                        if (st) {
                            strcat(result, st);
                            free(st);
                            mychomp(result);
                        }
                    }
                } else {
                        st = pmyMgr->suffix_check_morph(tmpword, tmpl, 0, NULL, aflag, needflag);
                        if (st) {
                            strcat(result, st);
                            free(st);
                            mychomp(result);
                        }
                }
                if (*result) return mystrdup(result);
            }
    }
    return NULL;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE

// get next homonym with same affix
struct hentry * SfxEntry::get_next_homonym(struct hentry * he, int optflags, AffEntry* ppfx, 
    const FLAG cclass, const FLAG needflag)
{
    PfxEntry* ep = (PfxEntry *) ppfx;
    FLAG eFlag = ep ? ep->getFlag() : FLAG_NULL;

    while (he->next_homonym) {
        he = he->next_homonym;
        if ((TESTAFF(he->astr, aflag, he->alen) || (ep && ep->getCont() && TESTAFF(ep->getCont(), aflag, ep->getContLen()))) && 
                            ((optflags & aeXPRODUCT) == 0 || 
                            TESTAFF(he->astr, eFlag, he->alen) ||
                             // handle conditional suffix
                            ((contclass) && TESTAFF(contclass, eFlag, contclasslen))
                            ) &&
                            // handle cont. class
                            ((!cclass) || 
                                ((contclass) && TESTAFF(contclass, cclass, contclasslen))
                            ) &&
                            // handle required flag
                            ((!needflag) || 
                              (TESTAFF(he->astr, needflag, he->alen) ||
                              ((contclass) && TESTAFF(contclass, needflag, contclasslen)))
                            )
                        ) return he;
    }
    return NULL;
}


#if 0

Appendix:  Understanding Affix Code


An affix is either a  prefix or a suffix attached to root words to make 
other words.

Basically a Prefix or a Suffix is set of AffEntry objects
which store information about the prefix or suffix along 
with supporting routines to check if a word has a particular 
prefix or suffix or a combination.

The structure affentry is defined as follows:

struct affentry
{
   unsigned short aflag;    // ID used to represent the affix
   char * strip;            // string to strip before adding affix
   char * appnd;            // the affix string to add
   unsigned char stripl;    // length of the strip string
   unsigned char appndl;    // length of the affix string
   char numconds;           // the number of conditions that must be met
   char opts;               // flag: aeXPRODUCT- combine both prefix and suffix 
   char   conds[SETSIZE];   // array which encodes the conditions to be met
};


Here is a suffix borrowed from the en_US.aff file.  This file 
is whitespace delimited.

SFX D Y 4 
SFX D   0     e          d
SFX D   y     ied        [^aeiou]y
SFX D   0     ed         [^ey]
SFX D   0     ed         [aeiou]y

This information can be interpreted as follows:

In the first line has 4 fields

Field
-----
1     SFX - indicates this is a suffix
2     D   - is the name of the character flag which represents this suffix
3     Y   - indicates it can be combined with prefixes (cross product)
4     4   - indicates that sequence of 4 affentry structures are needed to
               properly store the affix information

The remaining lines describe the unique information for the 4 SfxEntry 
objects that make up this affix.  Each line can be interpreted
as follows: (note fields 1 and 2 are as a check against line 1 info)

Field
-----
1     SFX         - indicates this is a suffix
2     D           - is the name of the character flag for this affix
3     y           - the string of chars to strip off before adding affix
                         (a 0 here indicates the NULL string)
4     ied         - the string of affix characters to add
5     [^aeiou]y   - the conditions which must be met before the affix
                    can be applied

Field 5 is interesting.  Since this is a suffix, field 5 tells us that
there are 2 conditions that must be met.  The first condition is that 
the next to the last character in the word must *NOT* be any of the 
following "a", "e", "i", "o" or "u".  The second condition is that
the last character of the word must end in "y".

So how can we encode this information concisely and be able to 
test for both conditions in a fast manner?  The answer is found
but studying the wonderful ispell code of Geoff Kuenning, et.al. 
(now available under a normal BSD license).

If we set up a conds array of 256 bytes indexed (0 to 255) and access it
using a character (cast to an unsigned char) of a string, we have 8 bits
of information we can store about that character.  Specifically we
could use each bit to say if that character is allowed in any of the 
last (or first for prefixes) 8 characters of the word.

Basically, each character at one end of the word (up to the number 
of conditions) is used to index into the conds array and the resulting 
value found there says whether the that character is valid for a 
specific character position in the word.  

For prefixes, it does this by setting bit 0 if that char is valid 
in the first position, bit 1 if valid in the second position, and so on. 

If a bit is not set, then that char is not valid for that postion in the
word.

If working with suffixes bit 0 is used for the character closest 
to the front, bit 1 for the next character towards the end, ..., 
with bit numconds-1 representing the last char at the end of the string. 

Note: since entries in the conds[] are 8 bits, only 8 conditions 
(read that only 8 character positions) can be examined at one
end of a word (the beginning for prefixes and the end for suffixes.

So to make this clearer, lets encode the conds array values for the 
first two affentries for the suffix D described earlier.


  For the first affentry:    
     numconds = 1             (only examine the last character)

     conds['e'] =  (1 << 0)   (the word must end in an E)
     all others are all 0

  For the second affentry:
     numconds = 2             (only examine the last two characters)     

     conds[X] = conds[X] | (1 << 0)     (aeiou are not allowed)
         where X is all characters *but* a, e, i, o, or u
         

     conds['y'] = (1 << 1)     (the last char must be a y)
     all other bits for all other entries in the conds array are zero


#endif

