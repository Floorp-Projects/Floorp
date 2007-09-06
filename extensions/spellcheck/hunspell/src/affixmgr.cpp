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

#include "affixmgr.hxx"
#include "affentry.hxx"
#include "langnum.hxx"

#include "csutil.hxx"

#ifndef MOZILLA_CLIENT
#ifndef W32
using namespace std;
#endif
#endif

AffixMgr::AffixMgr(const char * affpath, HashMgr* ptr) 
{
  // register hash manager and load affix data from aff file
  pHMgr = ptr;
  keystring = NULL;
  trystring = NULL;
  encoding=NULL;
  utf8 = 0;
  complexprefixes = 0;
  maptable = NULL;
  nummap = 0;
  breaktable = NULL;
  numbreak = 0;
  reptable = NULL;
  numrep = 0;
  checkcpdtable = NULL;
  numcheckcpd = 0;
  defcpdtable = NULL;
  numdefcpd = 0;
  phone = NULL;
  compoundflag = FLAG_NULL; // permits word in compound forms
  compoundbegin = FLAG_NULL; // may be first word in compound forms
  compoundmiddle = FLAG_NULL; // may be middle word in compound forms
  compoundend = FLAG_NULL; // may be last word in compound forms
  compoundroot = FLAG_NULL; // compound word signing flag
  compoundpermitflag = FLAG_NULL; // compound permitting flag for suffixed word
  compoundforbidflag = FLAG_NULL; // compound fordidden flag for suffixed word
  checkcompounddup = 0; // forbid double words in compounds
  checkcompoundrep = 0; // forbid bad compounds (may be non compound word with a REP substitution)
  checkcompoundcase = 0; // forbid upper and lowercase combinations at word bounds
  checkcompoundtriple = 0; // forbid compounds with triple letters
  forbiddenword = FLAG_NULL; // forbidden word signing flag
  nosuggest = FLAG_NULL; // don't suggest words signed with NOSUGGEST flag
  lang = NULL; // language
  langnum = 0; // language code (see http://l10n.openoffice.org/languages.html)
  pseudoroot = FLAG_NULL; // forbidden root, allowed only with suffixes
  cpdwordmax = -1; // default: unlimited wordcount in compound words
  cpdmin = -1;  // undefined
  cpdmaxsyllable = 0; // default: unlimited syllablecount in compound words
  cpdvowels=NULL; // vowels (for calculating of Hungarian compounding limit, O(n) search! XXX)
  cpdvowels_utf16=NULL; // vowels for UTF-8 encoding (bsearch instead of O(n) search)
  cpdvowels_utf16_len=0; // vowels
  pfxappnd=NULL; // previous prefix for counting the syllables of prefix BUG
  sfxappnd=NULL; // previous suffix for counting a special syllables BUG
  cpdsyllablenum=NULL; // syllable count incrementing flag
  checknum=0; // checking numbers, and word with numbers
  wordchars=NULL; // letters + spec. word characters
  wordchars_utf16=NULL; // letters + spec. word characters
  wordchars_utf16_len=0; // letters + spec. word characters
  ignorechars=NULL; // letters + spec. word characters
  ignorechars_utf16=NULL; // letters + spec. word characters
  ignorechars_utf16_len=0; // letters + spec. word characters
  version=NULL; // affix and dictionary file version string
  havecontclass=0; // flags of possible continuing classes (double affix)
  // LEMMA_PRESENT: not put root into the morphological output. Lemma presents
  // in morhological description in dictionary file. It's often combined with PSEUDOROOT.
  lemma_present = FLAG_NULL; 
  circumfix = FLAG_NULL; 
  onlyincompound = FLAG_NULL; 
  flag_mode = FLAG_CHAR; // default one-character flags in affix and dic file
  maxngramsugs = -1; // undefined
  nosplitsugs = 0;
  sugswithdots = 0;
  keepcase = 0;
  checksharps = 0;

  derived = NULL; // XXX not threadsafe variable for experimental stemming
  sfx = NULL;
  pfx = NULL;

  for (int i=0; i < SETSIZE; i++) {
     pStart[i] = NULL;
     sStart[i] = NULL;
     pFlag[i] = NULL;
     sFlag[i] = NULL;
  }

  for (int j=0; j < CONTSIZE; j++) {
    contclasses[j] = 0;
  }

  if (parse_file(affpath)) {
     HUNSPELL_WARNING(stderr, "Failure loading aff file %s\n",affpath);
  }
  
  if (cpdmin == -1) cpdmin = MINCPDLEN;

}


AffixMgr::~AffixMgr() 
{
 
  // pass through linked prefix entries and clean up
  for (int i=0; i < SETSIZE ;i++) {
       pFlag[i] = NULL;
       PfxEntry * ptr = (PfxEntry *)pStart[i];
       PfxEntry * nptr = NULL;
       while (ptr) {
            nptr = ptr->getNext();
            delete(ptr);
            ptr = nptr;
            nptr = NULL;
       }  
  }

  // pass through linked suffix entries and clean up
  for (int j=0; j < SETSIZE ; j++) {
       sFlag[j] = NULL;
       SfxEntry * ptr = (SfxEntry *)sStart[j];
       SfxEntry * nptr = NULL;
       while (ptr) {
            nptr = ptr->getNext();
            delete(ptr);
            ptr = nptr;
            nptr = NULL;
       }
       sStart[j] = NULL;
  }

  if (keystring) free(keystring);
  keystring=NULL;
  if (trystring) free(trystring);
  trystring=NULL;
  if (encoding) free(encoding);
  encoding=NULL;
  if (maptable) {  
     for (int j=0; j < nummap; j++) {
        if (maptable[j].set) free(maptable[j].set);
        if (maptable[j].set_utf16) free(maptable[j].set_utf16);
        maptable[j].set = NULL;
        maptable[j].len = 0;
     }
     free(maptable);  
     maptable = NULL;
  }
  nummap = 0;
  if (breaktable) {
     for (int j=0; j < numbreak; j++) {
        if (breaktable[j]) free(breaktable[j]);
        breaktable[j] = NULL;
     }
     free(breaktable);  
     breaktable = NULL;
  }
  numbreak = 0;
  if (reptable) {
     for (int j=0; j < numrep; j++) {
        free(reptable[j].pattern);
        free(reptable[j].pattern2);
     }
     free(reptable);  
     reptable = NULL;
  }
  if (phone && phone->rules) {
     for (int j=0; j < phone->num + 1; j++) {
        free(phone->rules[j * 2]);
        free(phone->rules[j * 2 + 1]);
     }
     free(phone->rules);
     free(phone);  
     phone = NULL;
  }

  if (defcpdtable) {  
     for (int j=0; j < numdefcpd; j++) {
        free(defcpdtable[j].def);
        defcpdtable[j].def = NULL;
     }
     free(defcpdtable);  
     defcpdtable = NULL;
  }
  numrep = 0;
  if (checkcpdtable) {  
     for (int j=0; j < numcheckcpd; j++) {
        free(checkcpdtable[j].pattern);
        free(checkcpdtable[j].pattern2);
        checkcpdtable[j].pattern = NULL;
        checkcpdtable[j].pattern2 = NULL;
     }
     free(checkcpdtable);  
     checkcpdtable = NULL;
  }
  numcheckcpd = 0;
  FREE_FLAG(compoundflag);
  FREE_FLAG(compoundbegin);
  FREE_FLAG(compoundmiddle);
  FREE_FLAG(compoundend);
  FREE_FLAG(compoundpermitflag);
  FREE_FLAG(compoundforbidflag);
  FREE_FLAG(compoundroot);
  FREE_FLAG(forbiddenword);
  FREE_FLAG(nosuggest);
  FREE_FLAG(pseudoroot);
  FREE_FLAG(lemma_present);
  FREE_FLAG(circumfix);
  FREE_FLAG(onlyincompound);
  
  cpdwordmax = 0;
  pHMgr = NULL;
  cpdmin = 0;
  cpdmaxsyllable = 0;
  if (cpdvowels) free(cpdvowels);
  if (cpdvowels_utf16) free(cpdvowels_utf16);
  if (cpdsyllablenum) free(cpdsyllablenum);
  free_utf_tbl();
  if (lang) free(lang);
  if (wordchars) free(wordchars);
  if (wordchars_utf16) free(wordchars_utf16);
  if (ignorechars) free(ignorechars);
  if (ignorechars_utf16) free(ignorechars_utf16);
  if (version) free(version);
  if (derived) free(derived);
  checknum=0;
}


// read in aff file and build up prefix and suffix entry objects 
int  AffixMgr::parse_file(const char * affpath)
{

  // io buffers
  char line[MAXLNLEN+1];
 
  // affix type
  char ft;
  
  // checking flag duplication
  char dupflags[CONTSIZE];
  char dupflags_ini = 1;

  // first line indicator for removing byte order mark
  int firstline = 1;
  
  // open the affix file
  FILE * afflst;
  afflst = fopen(affpath,"r");
  if (!afflst) {
    HUNSPELL_WARNING(stderr, "error: could not open affix description file %s\n",affpath);
    return 1;
  }

  // step one is to parse the affix file building up the internal
  // affix data structures


    // read in each line ignoring any that do not
    // start with a known line type indicator
    while (fgets(line,MAXLNLEN,afflst)) {
       mychomp(line);

       /* remove byte order mark */
       if (firstline) {
         firstline = 0;
         if (strncmp(line,"\xEF\xBB\xBF",3) == 0) {
            memmove(line, line+3, strlen(line+3)+1);
            HUNSPELL_WARNING(stderr, "warning: affix file begins with byte order mark: possible incompatibility with old Hunspell versions\n");
         }
       }

       /* parse in the keyboard string */
       if (strncmp(line,"KEY",3) == 0) {
          if (parse_string(line, &keystring, "KEY")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the try string */
       if (strncmp(line,"TRY",3) == 0) {
          if (parse_string(line, &trystring, "TRY")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the name of the character set used by the .dict and .aff */
       if (strncmp(line,"SET",3) == 0) {
          if (parse_string(line, &encoding, "SET")) {
             fclose(afflst);
             return 1;
          }
          if (strcmp(encoding, "UTF-8") == 0) {
             utf8 = 1;
#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
             if (initialize_utf_tbl()) return 1;
#endif
#endif
          }
       }

       /* parse COMPLEXPREFIXES for agglutinative languages with right-to-left writing system */
       if (strncmp(line,"COMPLEXPREFIXES",15) == 0)
                   complexprefixes = 1;

       /* parse in the flag used by the controlled compound words */
       if (strncmp(line,"COMPOUNDFLAG",12) == 0) {
          if (parse_flag(line, &compoundflag, "COMPOUNDFLAG")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by compound words */
       if (strncmp(line,"COMPOUNDBEGIN",13) == 0) {
          if (complexprefixes) {
            if (parse_flag(line, &compoundend, "COMPOUNDBEGIN")) {
              fclose(afflst);
              return 1;
            }
          } else {
            if (parse_flag(line, &compoundbegin, "COMPOUNDBEGIN")) {
              fclose(afflst);
              return 1;
            }
          }
       }

       /* parse in the flag used by compound words */
       if (strncmp(line,"COMPOUNDMIDDLE",14) == 0) {
          if (parse_flag(line, &compoundmiddle, "COMPOUNDMIDDLE")) {
             fclose(afflst);
             return 1;
          }
       }
       /* parse in the flag used by compound words */
       if (strncmp(line,"COMPOUNDEND",11) == 0) {
          if (complexprefixes) {
            if (parse_flag(line, &compoundbegin, "COMPOUNDEND")) {
              fclose(afflst);
              return 1;
            }
          } else {
            if (parse_flag(line, &compoundend, "COMPOUNDEND")) {
              fclose(afflst);
              return 1;
            }
          }
       }

       /* parse in the data used by compound_check() method */
       if (strncmp(line,"COMPOUNDWORDMAX",15) == 0) {
          if (parse_num(line, &cpdwordmax, "COMPOUNDWORDMAX")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag sign compounds in dictionary */
       if (strncmp(line,"COMPOUNDROOT",12) == 0) {
          if (parse_flag(line, &compoundroot, "COMPOUNDROOT")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by compound_check() method */
       if (strncmp(line,"COMPOUNDPERMITFLAG",18) == 0) {
          if (parse_flag(line, &compoundpermitflag, "COMPOUNDPERMITFLAG")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by compound_check() method */
       if (strncmp(line,"COMPOUNDFORBIDFLAG",18) == 0) {
          if (parse_flag(line, &compoundforbidflag, "COMPOUNDFORBIDFLAG")) {
             fclose(afflst);
             return 1;
          }
       }

       if (strncmp(line,"CHECKCOMPOUNDDUP",16) == 0) {
                   checkcompounddup = 1;
       }

       if (strncmp(line,"CHECKCOMPOUNDREP",16) == 0) {
                   checkcompoundrep = 1;
       }

       if (strncmp(line,"CHECKCOMPOUNDTRIPLE",19) == 0) {
                   checkcompoundtriple = 1;
       }

       if (strncmp(line,"CHECKCOMPOUNDCASE",17) == 0) {
                   checkcompoundcase = 1;
       }

       if (strncmp(line,"NOSUGGEST",9) == 0) {
          if (parse_flag(line, &nosuggest, "NOSUGGEST")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by forbidden words */
       if (strncmp(line,"FORBIDDENWORD",13) == 0) {
          if (parse_flag(line, &forbiddenword, "FORBIDDENWORD")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by forbidden words */
       if (strncmp(line,"LEMMA_PRESENT",13) == 0) {
          if (parse_flag(line, &lemma_present, "LEMMA_PRESENT")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by circumfixes */
       if (strncmp(line,"CIRCUMFIX",9) == 0) {
          if (parse_flag(line, &circumfix, "CIRCUMFIX")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by fogemorphemes */
       if (strncmp(line,"ONLYINCOMPOUND",14) == 0) {
          if (parse_flag(line, &onlyincompound, "ONLYINCOMPOUND")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by `pseudoroots' */
       if (strncmp(line,"PSEUDOROOT",10) == 0) {
          if (parse_flag(line, &pseudoroot, "PSEUDOROOT")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by `pseudoroots' */
       if (strncmp(line,"NEEDAFFIX",9) == 0) {
          if (parse_flag(line, &pseudoroot, "NEEDAFFIX")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the minimal length for words in compounds */
       if (strncmp(line,"COMPOUNDMIN",11) == 0) {
          if (parse_num(line, &cpdmin, "COMPOUNDMIN")) {
             fclose(afflst);
             return 1;
          }
          if (cpdmin < 1) cpdmin = 1;
       }

       /* parse in the max. words and syllables in compounds */
       if (strncmp(line,"COMPOUNDSYLLABLE",16) == 0) {
          if (parse_cpdsyllable(line)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by compound_check() method */
       if (strncmp(line,"SYLLABLENUM",11) == 0) {
          if (parse_string(line, &cpdsyllablenum, "SYLLABLENUM")) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the flag used by the controlled compound words */
       if (strncmp(line,"CHECKNUM",8) == 0) {
           checknum=1;
       }

       /* parse in the extra word characters */
       if (strncmp(line,"WORDCHARS",9) == 0) {
          if (parse_array(line, &wordchars, &wordchars_utf16, &wordchars_utf16_len, "WORDCHARS", utf8)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the ignored characters (for example, Arabic optional diacretics charachters */
       if (strncmp(line,"IGNORE",6) == 0) {
          if (parse_array(line, &ignorechars, &ignorechars_utf16, &ignorechars_utf16_len, "IGNORE", utf8)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the typical fault correcting table */
       if (strncmp(line,"REP",3) == 0) {
          if (parse_reptable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the phonetic translation table */
       if (strncmp(line,"PHONE",5) == 0) {
          if (parse_phonetable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the checkcompoundpattern table */
       if (strncmp(line,"CHECKCOMPOUNDPATTERN",20) == 0) {
          if (parse_checkcpdtable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the defcompound table */
       if (strncmp(line,"COMPOUNDRULE",12) == 0) {
          if (parse_defcpdtable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the related character map table */
       if (strncmp(line,"MAP",3) == 0) {
          if (parse_maptable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the word breakpoints table */
       if (strncmp(line,"BREAK",5) == 0) {
          if (parse_breaktable(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

       /* parse in the language for language specific codes */
       if (strncmp(line,"LANG",4) == 0) {
          if (parse_string(line, &lang, "LANG")) {
             fclose(afflst);
             return 1;
          }
          langnum = get_lang_num(lang);
       }

       if (strncmp(line,"VERSION",7) == 0) {
          if (parse_string(line, &version, "VERSION")) {
             fclose(afflst);
             return 1;
          }
       }

       if (strncmp(line,"MAXNGRAMSUGS",12) == 0) {
          if (parse_num(line, &maxngramsugs, "MAXNGRAMSUGS")) {
             fclose(afflst);
             return 1;
          }
       }

       if (strncmp(line,"NOSPLITSUGS",11) == 0) {
                   nosplitsugs=1;
       }

       if (strncmp(line,"SUGSWITHDOTS",12) == 0) {
                   sugswithdots=1;
       }

       /* parse in the flag used by forbidden words */
       if (strncmp(line,"KEEPCASE",8) == 0) {
          if (parse_flag(line, &keepcase, "KEEPCASE")) {
             fclose(afflst);
             return 1;
          }
       }

       if (strncmp(line,"CHECKSHARPS",11) == 0) {
                   checksharps=1;
       }

       /* parse this affix: P - prefix, S - suffix */
       ft = ' ';
       if (strncmp(line,"PFX",3) == 0) ft = complexprefixes ? 'S' : 'P';
       if (strncmp(line,"SFX",3) == 0) ft = complexprefixes ? 'P' : 'S';
       if (ft != ' ') {
          if (dupflags_ini) {
            for (int i = 0; i < CONTSIZE; i++) dupflags[i] = 0;
            dupflags_ini = 0;
          }
          if (parse_affix(line, ft, afflst, dupflags)) {
             fclose(afflst);
             process_pfx_tree_to_list();
             process_sfx_tree_to_list();
             return 1;
          }
       }

    }
    fclose(afflst);

    // convert affix trees to sorted list
    process_pfx_tree_to_list();
    process_sfx_tree_to_list();

    // now we can speed up performance greatly taking advantage of the 
    // relationship between the affixes and the idea of "subsets".

    // View each prefix as a potential leading subset of another and view
    // each suffix (reversed) as a potential trailing subset of another.

    // To illustrate this relationship if we know the prefix "ab" is found in the
    // word to examine, only prefixes that "ab" is a leading subset of need be examined.
    // Furthermore is "ab" is not present then none of the prefixes that "ab" is
    // is a subset need be examined.
    // The same argument goes for suffix string that are reversed.

    // Then to top this off why not examine the first char of the word to quickly
    // limit the set of prefixes to examine (i.e. the prefixes to examine must 
    // be leading supersets of the first character of the word (if they exist)
 
    // To take advantage of this "subset" relationship, we need to add two links
    // from entry.  One to take next if the current prefix is found (call it nexteq)
    // and one to take next if the current prefix is not found (call it nextne).

    // Since we have built ordered lists, all that remains is to properly intialize 
    // the nextne and nexteq pointers that relate them

    process_pfx_order();
    process_sfx_order();

    /* get encoding for CHECKCOMPOUNDCASE */
    char * enc = get_encoding();
    csconv = get_current_cs(enc);
    free(enc);
    enc = NULL;

    // temporary BREAK definition for German dash handling (OOo issue 64400)
    if ((langnum == LANG_de) && (!breaktable)) {
        breaktable = (char **) malloc(sizeof(char *));
        if (!breaktable) return 1;
        breaktable[0] = mystrdup("-");
        numbreak = 1;
    }
    return 0;
}


// we want to be able to quickly access prefix information
// both by prefix flag, and sorted by prefix string itself 
// so we need to set up two indexes

int AffixMgr::build_pfxtree(AffEntry* pfxptr)
{
  PfxEntry * ptr;
  PfxEntry * pptr;
  PfxEntry * ep = (PfxEntry*) pfxptr;

  // get the right starting points
  const char * key = ep->getKey();
  const unsigned char flg = (unsigned char) (ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = (PfxEntry*)pFlag[flg];
  ep->setFlgNxt(ptr);
  pFlag[flg] = (AffEntry *) ep;


  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
     ptr = (PfxEntry*)pStart[0];
     ep->setNext(ptr);
     pStart[0] = (AffEntry*)ep;
     return 0;
  }

  // now handle the normal case
  ep->setNextEQ(NULL);
  ep->setNextNE(NULL);

  unsigned char sp = *((const unsigned char *)key);
  ptr = (PfxEntry*)pStart[sp];
  
  // handle the first insert 
  if (!ptr) {
     pStart[sp] = (AffEntry*)ep;
     return 0;
  }


  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = NULL;
  for (;;) {
    pptr = ptr;
    if (strcmp(ep->getKey(), ptr->getKey() ) <= 0) {
       ptr = ptr->getNextEQ();
       if (!ptr) {
          pptr->setNextEQ(ep);
          break;
       }
    } else {
       ptr = ptr->getNextNE();
       if (!ptr) {
          pptr->setNextNE(ep);
          break;
       }
    }
  }
  return 0;
}

// we want to be able to quickly access suffix information
// both by suffix flag, and sorted by the reverse of the
// suffix string itself; so we need to set up two indexes
int AffixMgr::build_sfxtree(AffEntry* sfxptr)
{
  SfxEntry * ptr;
  SfxEntry * pptr;
  SfxEntry * ep = (SfxEntry *) sfxptr;

  /* get the right starting point */
  const char * key = ep->getKey();
  const unsigned char flg = (unsigned char) (ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = (SfxEntry*)sFlag[flg];
  ep->setFlgNxt(ptr);
  sFlag[flg] = (AffEntry *) ep;

  // next index by affix string

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
     ptr = (SfxEntry*)sStart[0];
     ep->setNext(ptr);
     sStart[0] = (AffEntry*)ep;
     return 0;
  }

  // now handle the normal case
  ep->setNextEQ(NULL);
  ep->setNextNE(NULL);

  unsigned char sp = *((const unsigned char *)key);
  ptr = (SfxEntry*)sStart[sp];
  
  // handle the first insert 
  if (!ptr) {
     sStart[sp] = (AffEntry*)ep;
     return 0;
  }

  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = NULL;
  for (;;) {
    pptr = ptr;
    if (strcmp(ep->getKey(), ptr->getKey() ) <= 0) {
       ptr = ptr->getNextEQ();
       if (!ptr) {
          pptr->setNextEQ(ep);
          break;
       }
    } else {
       ptr = ptr->getNextNE();
       if (!ptr) {
          pptr->setNextNE(ep);
          break;
       }
    }
  }
  return 0;
}

// convert from binary tree to sorted list
int AffixMgr::process_pfx_tree_to_list()
{
  for (int i=1; i< SETSIZE; i++) {
    pStart[i] = process_pfx_in_order(pStart[i],NULL);
  }
  return 0;
}


AffEntry* AffixMgr::process_pfx_in_order(AffEntry* ptr, AffEntry* nptr)
{
  if (ptr) {
    nptr = process_pfx_in_order(((PfxEntry*) ptr)->getNextNE(), nptr);
    ((PfxEntry*) ptr)->setNext((PfxEntry*) nptr);
    nptr = process_pfx_in_order(((PfxEntry*) ptr)->getNextEQ(), ptr);
  }
  return nptr;
}


// convert from binary tree to sorted list
int AffixMgr:: process_sfx_tree_to_list()
{
  for (int i=1; i< SETSIZE; i++) {
    sStart[i] = process_sfx_in_order(sStart[i],NULL);
  }
  return 0;
}

AffEntry* AffixMgr::process_sfx_in_order(AffEntry* ptr, AffEntry* nptr)
{
  if (ptr) {
    nptr = process_sfx_in_order(((SfxEntry*) ptr)->getNextNE(), nptr);
    ((SfxEntry*) ptr)->setNext((SfxEntry*) nptr);
    nptr = process_sfx_in_order(((SfxEntry*) ptr)->getNextEQ(), ptr);
  }
  return nptr;
}


// reinitialize the PfxEntry links NextEQ and NextNE to speed searching
// using the idea of leading subsets this time
int AffixMgr::process_pfx_order()
{
    PfxEntry* ptr;

    // loop through each prefix list starting point
    for (int i=1; i < SETSIZE; i++) {

         ptr = (PfxEntry*)pStart[i];

         // look through the remainder of the list
         //  and find next entry with affix that 
         // the current one is not a subset of
         // mark that as destination for NextNE
         // use next in list that you are a subset
         // of as NextEQ

         for (; ptr != NULL; ptr = ptr->getNext()) {

             PfxEntry * nptr = ptr->getNext();
             for (; nptr != NULL; nptr = nptr->getNext()) {
                 if (! isSubset( ptr->getKey() , nptr->getKey() )) break;
             }
             ptr->setNextNE(nptr);
             ptr->setNextEQ(NULL);
             if ((ptr->getNext()) && isSubset(ptr->getKey() , (ptr->getNext())->getKey())) 
                 ptr->setNextEQ(ptr->getNext());
         }

         // now clean up by adding smart search termination strings:
         // if you are already a superset of the previous prefix
         // but not a subset of the next, search can end here
         // so set NextNE properly

         ptr = (PfxEntry *) pStart[i];
         for (; ptr != NULL; ptr = ptr->getNext()) {
             PfxEntry * nptr = ptr->getNext();
             PfxEntry * mptr = NULL;
             for (; nptr != NULL; nptr = nptr->getNext()) {
                 if (! isSubset(ptr->getKey(),nptr->getKey())) break;
                 mptr = nptr;
             }
             if (mptr) mptr->setNextNE(NULL);
         }
    }
    return 0;
}

// initialize the SfxEntry links NextEQ and NextNE to speed searching
// using the idea of leading subsets this time
int AffixMgr::process_sfx_order()
{
    SfxEntry* ptr;

    // loop through each prefix list starting point
    for (int i=1; i < SETSIZE; i++) {

         ptr = (SfxEntry *) sStart[i];

         // look through the remainder of the list
         //  and find next entry with affix that 
         // the current one is not a subset of
         // mark that as destination for NextNE
         // use next in list that you are a subset
         // of as NextEQ

         for (; ptr != NULL; ptr = ptr->getNext()) {
             SfxEntry * nptr = ptr->getNext();
             for (; nptr != NULL; nptr = nptr->getNext()) {
                 if (! isSubset(ptr->getKey(),nptr->getKey())) break;
             }
             ptr->setNextNE(nptr);
             ptr->setNextEQ(NULL);
             if ((ptr->getNext()) && isSubset(ptr->getKey(),(ptr->getNext())->getKey())) 
                 ptr->setNextEQ(ptr->getNext());
         }


         // now clean up by adding smart search termination strings:
         // if you are already a superset of the previous suffix
         // but not a subset of the next, search can end here
         // so set NextNE properly

         ptr = (SfxEntry *) sStart[i];
         for (; ptr != NULL; ptr = ptr->getNext()) {
             SfxEntry * nptr = ptr->getNext();
             SfxEntry * mptr = NULL;
             for (; nptr != NULL; nptr = nptr->getNext()) {
                 if (! isSubset(ptr->getKey(),nptr->getKey())) break;
                 mptr = nptr;
             }
             if (mptr) mptr->setNextNE(NULL);
         }
    }
    return 0;
}



// takes aff file condition string and creates the
// conds array - please see the appendix at the end of the
// file affentry.cpp which describes what is going on here
// in much more detail

int AffixMgr::encodeit(struct affentry * ptr, char * cs)
{
  unsigned char c;
  int i, j, k;
  unsigned char mbr[MAXLNLEN];
  w_char wmbr[MAXLNLEN];
  w_char * wpos = wmbr;

  // now clear the conditions array */
  for (i=0;i<SETSIZE;i++) ptr->conds.base[i] = (unsigned char) 0;

  // now parse the string to create the conds array */
  int nc = strlen(cs);
  unsigned char neg = 0;   // complement indicator
  int grp = 0;   // group indicator
  unsigned char n = 0;     // number of conditions
  int ec = 0;    // end condition indicator
  int nm = 0;    // number of member in group

  // if no condition just return
  if (strcmp(cs,".")==0) {
    ptr->numconds = 0;
    return 0;
  }

  i = 0;
  while (i < nc) {
    c = *((unsigned char *)(cs + i));

    // start group indicator
    if (c == '[') {
       grp = 1;
       c = 0;
    }

    // complement flag
    if ((grp == 1) && (c == '^')) {
       neg = 1;
       c = 0;
    }

    // end goup indicator
    if (c == ']') {
       ec = 1;
       c = 0;
    }

    // add character of group to list
    if ((grp == 1) && (c != 0)) {
      *(mbr + nm) = c;
      nm++;
      c = 0;
    }

    // end of condition 
    if (c != 0) {
       ec = 1;
    }

  if (ec) {    
    if (!utf8) {
      if (grp == 1) {
        if (neg == 0) {
          // set the proper bits in the condition array vals for those chars
          for (j=0;j<nm;j++) {
             k = (unsigned int) mbr[j];
             ptr->conds.base[k] = ptr->conds.base[k] | ((unsigned char)1 << n);
          }
        } else {
          // complement so set all of them and then unset indicated ones
           for (j=0;j<SETSIZE;j++) ptr->conds.base[j] = ptr->conds.base[j] | ((unsigned char)1 << n);
           for (j=0;j<nm;j++) {
             k = (unsigned int) mbr[j];
             ptr->conds.base[k] = ptr->conds.base[k] & ~((unsigned char)1 << n);
           }
        }
        neg = 0;
        grp = 0;   
        nm = 0;
      } else {
         // not a group so just set the proper bit for this char
         // but first handle special case of . inside condition
         if (c == '.') {
            // wild card character so set them all
            for (j=0;j<SETSIZE;j++) ptr->conds.base[j] = ptr->conds.base[j] | ((unsigned char)1 << n);
         } else {  
            ptr->conds.base[(unsigned int) c] = ptr->conds.base[(unsigned int)c] | ((unsigned char)1 << n);
         }
      }
      n++;
      ec = 0;
    } else { // UTF-8 character set
      if (grp == 1) {
        ptr->conds.utf8.neg[n] = neg;
        if (neg == 0) {
          // set the proper bits in the condition array vals for those chars
          for (j=0;j<nm;j++) {
             k = (unsigned int) mbr[j];
             if (k >> 7) {
                u8_u16(wpos, 1, (char *) mbr + j);
                wpos++;
                if ((k & 0xe0) == 0xe0) j+=2; else j++; // 3-byte UTF-8 character
             } else {
                ptr->conds.utf8.ascii[k] = ptr->conds.utf8.ascii[k] | ((unsigned char)1 << n);
             }
          }
        } else { // neg == 1
          // complement so set all of them and then unset indicated ones
           for (j=0;j<(SETSIZE/2);j++) ptr->conds.utf8.ascii[j] = ptr->conds.utf8.ascii[j] | ((unsigned char)1 << n);
           for (j=0;j<nm;j++) {
             k = (unsigned int) mbr[j];
             if (k >> 7) {
                u8_u16(wpos, 1, (char *) mbr + j);
                wpos++;
                if ((k & 0xe0) == 0xe0) j+=2; else j++; // 3-byte UTF-8 character
             } else {
                ptr->conds.utf8.ascii[k] = ptr->conds.utf8.ascii[k] & ~((unsigned char)1 << n);
             }
           }
        }
        neg = 0;
        grp = 0;   
        nm = 0;
        ptr->conds.utf8.wlen[n] = wpos - wmbr;
        if ((wpos - wmbr) != 0) {
            ptr->conds.utf8.wchars[n] = (w_char *) malloc(sizeof(w_char) * (wpos - wmbr));
            if (!ptr->conds.utf8.wchars[n]) return 1;
            memcpy(ptr->conds.utf8.wchars[n], wmbr, sizeof(w_char) * (wpos - wmbr));
            flag_qsort((unsigned short *) ptr->conds.utf8.wchars[n], 0, ptr->conds.utf8.wlen[n]);
            wpos = wmbr;
        }
      } else { // grp == 0
         // is UTF-8 character?
         if (c >> 7) {
            ptr->conds.utf8.wchars[n] = (w_char *) malloc(sizeof(w_char));
            if (!ptr->conds.utf8.wchars[n]) return 1;
            ptr->conds.utf8.wlen[n] = 1;
            u8_u16(ptr->conds.utf8.wchars[n], 1, cs + i);
            if ((c & 0xe0) == 0xe0) i+=2; else i++; // 3-byte UFT-8 character
         } else {
            ptr->conds.utf8.wchars[n] = NULL;
            // not a group so just set the proper bit for this char
            // but first handle special case of . inside condition
            if (c == '.') {
                ptr->conds.utf8.all[n] = 1;
                // wild card character so set them all
                for (j=0;j<(SETSIZE/2);j++) ptr->conds.utf8.ascii[j] = ptr->conds.utf8.ascii[j] | ((unsigned char)1 << n);
            } else {
                ptr->conds.utf8.all[n] = 0;
                ptr->conds.utf8.ascii[(unsigned int) c] = ptr->conds.utf8.ascii[(unsigned int)c] | ((unsigned char)1 << n);
            }
         }
         neg = 0;
      }
      n++;
      ec = 0;
      neg = 0;
    }  
  }

    i++;
  }
  ptr->numconds = n;
  return 0;
}

 // return 1 if s1 is a leading subset of s2
/* inline int AffixMgr::isSubset(const char * s1, const char * s2)
 {
    while ((*s1 == *s2) && *s1) {
        s1++;
        s2++;
    }
    return (*s1 == '\0');
 }
*/

 // return 1 if s1 is a leading subset of s2 (dots are for infixes)
inline int AffixMgr::isSubset(const char * s1, const char * s2)
 {
    while (((*s1 == *s2) || (*s1 == '.')) && (*s1 != '\0')) {
        s1++;
        s2++;
    }
    return (*s1 == '\0');
 }


// check word for prefixes
struct hentry * AffixMgr::prefix_check(const char * word, int len, char in_compound,
    const FLAG needflag)
{
    struct hentry * rv= NULL;

    pfx = NULL;
    pfxappnd = NULL;
    sfxappnd = NULL;
    
    // first handle the special case of 0 length prefixes
    PfxEntry * pe = (PfxEntry *) pStart[0];
    while (pe) {
        if (
            // fogemorpheme
              ((in_compound != IN_CPD_NOT) || !(pe->getCont() &&
                  (TESTAFF(pe->getCont(), onlyincompound, pe->getContLen())))) &&
            // permit prefixes in compounds
              ((in_compound != IN_CPD_END) || (pe->getCont() &&
                  (TESTAFF(pe->getCont(), compoundpermitflag, pe->getContLen()))))
              ) {
                    // check prefix
                    rv = pe->checkword(word, len, in_compound, needflag);
                    if (rv) {
                        pfx=(AffEntry *)pe; // BUG: pfx not stateless
                        return rv;
                    }
             }
       pe = pe->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)word);
    PfxEntry * pptr = (PfxEntry *)pStart[sp];

    while (pptr) {
        if (isSubset(pptr->getKey(),word)) {
             if (
            // fogemorpheme
              ((in_compound != IN_CPD_NOT) || !(pptr->getCont() &&
                  (TESTAFF(pptr->getCont(), onlyincompound, pptr->getContLen())))) &&
            // permit prefixes in compounds
              ((in_compound != IN_CPD_END) || (pptr->getCont() &&
                  (TESTAFF(pptr->getCont(), compoundpermitflag, pptr->getContLen()))))
              ) {
            // check prefix
                  rv = pptr->checkword(word, len, in_compound, needflag);
                  if (rv) {
                    pfx=(AffEntry *)pptr; // BUG: pfx not stateless
                    return rv;
                  }
             }
             pptr = pptr->getNextEQ();
        } else {
             pptr = pptr->getNextNE();
        }
    }
    
    return NULL;
}

// check word for prefixes
struct hentry * AffixMgr::prefix_check_twosfx(const char * word, int len,
    char in_compound, const FLAG needflag)
{
    struct hentry * rv= NULL;

    pfx = NULL;
    sfxappnd = NULL;
    
    // first handle the special case of 0 length prefixes
    PfxEntry * pe = (PfxEntry *) pStart[0];
    
    while (pe) {
        rv = pe->check_twosfx(word, len, in_compound, needflag);
        if (rv) return rv;
        pe = pe->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)word);
    PfxEntry * pptr = (PfxEntry *)pStart[sp];

    while (pptr) {
        if (isSubset(pptr->getKey(),word)) {
            rv = pptr->check_twosfx(word, len, in_compound, needflag);
            if (rv) {
                pfx = (AffEntry *)pptr;
                return rv;
            }
            pptr = pptr->getNextEQ();
        } else {
             pptr = pptr->getNextNE();
        }
    }
    
    return NULL;
}

#ifdef HUNSPELL_EXPERIMENTAL
// check word for prefixes
char * AffixMgr::prefix_check_morph(const char * word, int len, char in_compound,
    const FLAG needflag)
{
    char * st;

    char result[MAXLNLEN];
    result[0] = '\0';

    pfx = NULL;
    sfxappnd = NULL;
    
    // first handle the special case of 0 length prefixes
    PfxEntry * pe = (PfxEntry *) pStart[0];
    while (pe) {
       st = pe->check_morph(word,len,in_compound, needflag);
       if (st) {
            strcat(result, st);
            free(st);
       }
       // if (rv) return rv;
       pe = pe->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)word);
    PfxEntry * pptr = (PfxEntry *)pStart[sp];

    while (pptr) {
        if (isSubset(pptr->getKey(),word)) {
            st = pptr->check_morph(word,len,in_compound, needflag);
            if (st) {
              // fogemorpheme
              if ((in_compound != IN_CPD_NOT) || !((pptr->getCont() && 
                        (TESTAFF(pptr->getCont(), onlyincompound, pptr->getContLen()))))) {
                    strcat(result, st);
                    pfx = (AffEntry *)pptr;
                }
                free(st);
            }
            pptr = pptr->getNextEQ();
        } else {
            pptr = pptr->getNextNE();
        }
    }
    
    if (*result) return mystrdup(result);
    return NULL;
}


// check word for prefixes
char * AffixMgr::prefix_check_twosfx_morph(const char * word, int len,
    char in_compound, const FLAG needflag)
{
    char * st;

    char result[MAXLNLEN];
    result[0] = '\0';

    pfx = NULL;
    sfxappnd = NULL;
    
    // first handle the special case of 0 length prefixes
    PfxEntry * pe = (PfxEntry *) pStart[0];
    while (pe) {
        st = pe->check_twosfx_morph(word,len,in_compound, needflag);
        if (st) {
            strcat(result, st);
            free(st);
        }
        pe = pe->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)word);
    PfxEntry * pptr = (PfxEntry *)pStart[sp];

    while (pptr) {
        if (isSubset(pptr->getKey(),word)) {
            st = pptr->check_twosfx_morph(word, len, in_compound, needflag);
            if (st) {
                strcat(result, st);
                free(st);
                pfx = (AffEntry *)pptr;
            }
            pptr = pptr->getNextEQ();
        } else {
            pptr = pptr->getNextNE();
        }
    }
    
    if (*result) return mystrdup(result);
    return NULL;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE


// Is word a non compound with a REP substitution (see checkcompoundrep)?
int AffixMgr::cpdrep_check(const char * word, int wl)
{
  char candidate[MAXLNLEN];
  const char * r;
  int lenr, lenp;

  if ((wl < 2) || !numrep) return 0;

  for (int i=0; i < numrep; i++ ) {
      r = word;
      lenr = strlen(reptable[i].pattern2);
      lenp = strlen(reptable[i].pattern);
      // search every occurence of the pattern in the word
      while ((r=strstr(r, reptable[i].pattern)) != NULL) {
          strcpy(candidate, word);
          if (r-word + lenr + strlen(r+lenp) >= MAXLNLEN) break;
          strcpy(candidate+(r-word),reptable[i].pattern2);
          strcpy(candidate+(r-word)+lenr, r+lenp);
          if (candidate_check(candidate,strlen(candidate))) return 1;
          r++; // search for the next letter
      }
   }
   return 0;
}

// forbid compoundings when there are special patterns at word bound
int AffixMgr::cpdpat_check(const char * word, int pos)
{
  int len;
  for (int i = 0; i < numcheckcpd; i++) {
      if (isSubset(checkcpdtable[i].pattern2, word + pos) &&
        (len = strlen(checkcpdtable[i].pattern)) && (pos > len) &&
        (strncmp(word + pos - len, checkcpdtable[i].pattern, len) == 0)) return 1;
  }
  return 0;
}

// forbid compounding with neighbouring upper and lower case characters at word bounds
int AffixMgr::cpdcase_check(const char * word, int pos)
{
  if (utf8) {
      w_char u, w;
      const char * p;
      u8_u16(&u, 1, word + pos);
      for (p = word + pos - 1; (*p & 0xc0) == 0x80; p--);
      u8_u16(&w, 1, p);
      unsigned short a = (u.h << 8) + u.l;
      unsigned short b = (w.h << 8) + w.l;
      if (((unicodetoupper(a, langnum) == a) || (unicodetoupper(b, langnum) == b))) return 1;
  } else {
      unsigned char a = *(word + pos - 1);
      unsigned char b = *(word + pos);
      if ((csconv[a].ccase || csconv[b].ccase) && (a != '-') && (b != '-')) return 1;
  }
  return 0;
}

// check compound patterns
int AffixMgr::defcpd_check(hentry *** words, short wnum, hentry * rv, hentry ** def, char all)
{
  signed short btpp[MAXWORDLEN]; // metacharacter (*, ?) positions for backtracking
  signed short btwp[MAXWORDLEN]; // word positions for metacharacters
  int btnum[MAXWORDLEN]; // number of matched characters in metacharacter positions
  short bt = 0;  
  int i;
  int ok;
  int w = 0;
  if (!*words) {
    w = 1;
    *words = def;
  }
  (*words)[wnum] = rv;

  for (i = 0; i < numdefcpd; i++) {
    signed short pp = 0; // pattern position
    signed short wp = 0; // "words" position
    int ok2;
    ok = 1;
    ok2 = 1;
    do {
      while ((pp < defcpdtable[i].len) && (wp <= wnum)) {
        if (((pp+1) < defcpdtable[i].len) &&
          ((defcpdtable[i].def[pp+1] == '*') || (defcpdtable[i].def[pp+1] == '?'))) {
            int wend = (defcpdtable[i].def[pp+1] == '?') ? wp : wnum;
            ok2 = 1;
            pp+=2;
            btpp[bt] = pp;
            btwp[bt] = wp;
            while (wp <= wend) {
                if (!(*words)[wp]->alen || 
                  !TESTAFF((*words)[wp]->astr, defcpdtable[i].def[pp-2], (*words)[wp]->alen)) {
                    ok2 = 0;
                    break;
                }
                wp++;
            }
            if (wp <= wnum) ok2 = 0;
            btnum[bt] = wp - btwp[bt];
            if (btnum[bt] > 0) bt++;
            if (ok2) break;
        } else {
            ok2 = 1;
            if (!(*words)[wp] || !(*words)[wp]->alen || 
              !TESTAFF((*words)[wp]->astr, defcpdtable[i].def[pp], (*words)[wp]->alen)) {
                ok = 0;
                break;
            }
            pp++;
            wp++;
            if ((defcpdtable[i].len == pp) && !(wp > wnum)) ok = 0;
        }
      }
    if (ok && ok2) { 
        int r = pp;
        while ((defcpdtable[i].len > r) && ((r+1) < defcpdtable[i].len) &&
            ((defcpdtable[i].def[r+1] == '*') || (defcpdtable[i].def[r+1] == '?'))) r+=2;
        if (defcpdtable[i].len <= r) return 1;
    }    
    // backtrack
    if (bt) do {
        ok = 1;
        btnum[bt - 1]--;
        pp = btpp[bt - 1];
        wp = btwp[bt - 1] + btnum[bt - 1];
    } while ((btnum[bt - 1] < 0) && --bt);
  } while (bt);

  if (ok && ok2 && (!all || (defcpdtable[i].len <= pp))) return 1; 
  // check zero ending
  while (ok && ok2 && (defcpdtable[i].len > pp) && ((pp+1) < defcpdtable[i].len) &&
    ((defcpdtable[i].def[pp+1] == '*') || (defcpdtable[i].def[pp+1] == '?'))) pp+=2;
  if (ok && ok2 && (defcpdtable[i].len <= pp)) return 1;
  }
  (*words)[wnum] = NULL;
  if (w) *words = NULL;
  return 0;
}

inline int AffixMgr::candidate_check(const char * word, int len)
{
  struct hentry * rv=NULL;
  
  rv = lookup(word);
  if (rv) return 1;

//  rv = prefix_check(word,len,1);
//  if (rv) return 1;
  
  rv = affix_check(word,len);
  if (rv) return 1;
  return 0;
}

// calculate number of syllable for compound-checking
short AffixMgr::get_syllable(const char * word, int wlen)
{
    if (cpdmaxsyllable==0) return 0;
    
    short num=0;

    if (!utf8) {
        for (int i=0; i<wlen; i++) {
            if (strchr(cpdvowels, word[i])) num++;
        }
    } else if (cpdvowels_utf16) {
        w_char w[MAXWORDUTF8LEN];
        int i = u8_u16(w, MAXWORDUTF8LEN, word);
        for (; i > 0; i--) {
            if (flag_bsearch((unsigned short *) cpdvowels_utf16,
                ((unsigned short *) w)[i - 1], cpdvowels_utf16_len)) num++;
        }
    }
    return num;
}

// check if compound word is correctly spelled
// hu_mov_rule = spec. Hungarian rule (XXX)
struct hentry * AffixMgr::compound_check(const char * word, int len, 
    short wordnum, short numsyllable, short maxwordnum, short wnum, hentry ** words = NULL,
    char hu_mov_rule = 0, int * cmpdstemnum = NULL, int * cmpdstem = NULL, char is_sug = 0)
{
    int i; 
    short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
    int oldcmpdstemnum = 0;
    struct hentry * rv = NULL;
    struct hentry * rv_first;
    struct hentry * rwords[MAXWORDLEN]; // buffer for COMPOUND pattern checking
    char st [MAXWORDUTF8LEN + 4];
    char ch;
    int cmin;
    int cmax;
    
    int checked_prefix;

#ifdef HUNSTEM
    if (cmpdstemnum) {
        if (wordnum == 0) {
            *cmpdstemnum = 1;
        } else {
            (*cmpdstemnum)++;
        }
    }
#endif
    if (utf8) {
        for (cmin = 0, i = 0; (i < cpdmin) && word[cmin]; i++) {
          cmin++;
          for (; (word[cmin] & 0xc0) == 0x80; cmin++);
        }
        for (cmax = len, i = 0; (i < (cpdmin - 1)) && cmax; i++) {
          cmax--;
          for (; (word[cmax] & 0xc0) == 0x80; cmax--);
        }
    } else {
        cmin = cpdmin;
        cmax = len - cpdmin + 1;
    }

    strcpy(st, word);

    for (i = cmin; i < cmax; i++) {

        oldnumsyllable = numsyllable;
        oldwordnum = wordnum;
        checked_prefix = 0;

        // go to end of the UTF-8 character
        if (utf8) {
            for (; (st[i] & 0xc0) == 0x80; i++);
            if (i >= cmax) return NULL;
        }

        
        ch = st[i];
        st[i] = '\0';

        sfx = NULL;
        pfx = NULL;
        
        // FIRST WORD
        
        rv = lookup(st); // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && !hu_mov_rule &&
            ((pseudoroot && TESTAFF(rv->astr, pseudoroot, rv->alen)) ||
                !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                  (compoundbegin && !wordnum &&
                        TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
                  (compoundmiddle && wordnum && !words &&
                    TESTAFF(rv->astr, compoundmiddle, rv->alen)) ||
                  (numdefcpd &&
                    ((!words && !wordnum && defcpd_check(&words, wnum, rv, (hentry **) &rwords, 0)) ||
                    (words && defcpd_check(&words, wnum, rv, (hentry **) &rwords, 0))))
                  ))) {
            rv = rv->next_homonym;
        }

        if (!rv) {
            if (compoundflag && 
             !(rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundflag))) {
                if ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL,
                        FLAG_NULL, compoundflag, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) && !hu_mov_rule &&
                    ((SfxEntry*)sfx)->getCont() &&
                        ((compoundforbidflag && TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                            ((SfxEntry*)sfx)->getContLen())) || (compoundend &&
                        TESTAFF(((SfxEntry*)sfx)->getCont(), compoundend, 
                            ((SfxEntry*)sfx)->getContLen())))) {
                        rv = NULL;
                }
            }
            if (rv ||
              (((wordnum == 0) && compoundbegin &&
                ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL, FLAG_NULL, compoundbegin, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                (rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundbegin)))) ||
              ((wordnum > 0) && compoundmiddle &&
                ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL, FLAG_NULL, compoundmiddle, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                (rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundmiddle)))))
              ) checked_prefix = 1;
        // else check forbiddenwords and pseudoroot
        } else if (rv->astr && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
            TESTAFF(rv->astr, pseudoroot, rv->alen) || 
            (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen))
             )) {
                st[i] = ch;
                continue;
        }

            // check non_compound flag in suffix and prefix
            if ((rv) && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundforbidflag, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }

            // check compoundend flag in suffix and prefix
            if ((rv) && !checked_prefix && compoundend && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundend, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundend, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }
            
            // check compoundmiddle flag in suffix and prefix
            if ((rv) && !checked_prefix && (wordnum==0) && compoundmiddle && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundmiddle, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundmiddle, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }       

        // check forbiddenwords
        if ((rv) && (rv->astr) && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
            (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen)))) {
                return NULL;
            }

        // increment word number, if the second root has a compoundroot flag
        if ((rv) && compoundroot && 
            (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                wordnum++;
        }

        // first word is acceptable in compound words?
        if (((rv) && 
          ( checked_prefix || (words && words[wnum]) ||
            (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
            ((oldwordnum == 0) && compoundbegin && TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
            ((oldwordnum > 0) && compoundmiddle && TESTAFF(rv->astr, compoundmiddle, rv->alen))// ||
//            (numdefcpd && )

// LANG_hu section: spec. Hungarian rule
            || ((langnum == LANG_hu) && hu_mov_rule && (
                    TESTAFF(rv->astr, 'F', rv->alen) || // XXX hardwired Hungarian dictionary codes
                    TESTAFF(rv->astr, 'G', rv->alen) ||
                    TESTAFF(rv->astr, 'H', rv->alen)
                )
              )
// END of LANG_hu section
          )
          && ! (( checkcompoundtriple && // test triple letters
                   (word[i-1]==word[i]) && (
                      ((i>1) && (word[i-1]==word[i-2])) || 
                      ((word[i-1]==word[i+1])) // may be word[i+1] == '\0'
                   )
               ) ||
               ( 
                 // test CHECKCOMPOUNDPATTERN
                 numcheckcpd && cpdpat_check(word, i)
               ) ||
               ( 
                 checkcompoundcase && cpdcase_check(word, i)
               ))
         )
// LANG_hu section: spec. Hungarian rule
         || ((!rv) && (langnum == LANG_hu) && hu_mov_rule && (rv = affix_check(st,i)) &&
              (sfx && ((SfxEntry*)sfx)->getCont() && ( // XXX hardwired Hungarian dic. codes
                        TESTAFF(((SfxEntry*)sfx)->getCont(), (unsigned short) 'x', ((SfxEntry*)sfx)->getContLen()) ||
                        TESTAFF(((SfxEntry*)sfx)->getCont(), (unsigned short) '%', ((SfxEntry*)sfx)->getContLen())
                    )                
               )
             )
// END of LANG_hu section
         ) {

// LANG_hu section: spec. Hungarian rule
            if (langnum == LANG_hu) {
                // calculate syllable number of the word            
                numsyllable += get_syllable(st, i);

                // + 1 word, if syllable number of the prefix > 1 (hungarian convention)
                if (pfx && (get_syllable(((PfxEntry *)pfx)->getKey(),strlen(((PfxEntry *)pfx)->getKey())) > 1)) wordnum++;
            }
// END of LANG_hu section

#ifdef HUNSTEM
            if (cmpdstem) cmpdstem[*cmpdstemnum - 1] = i;
#endif

            // NEXT WORD(S)
            rv_first = rv;
            rv = lookup((word+i)); // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && ((pseudoroot && TESTAFF(rv->astr, pseudoroot, rv->alen)) ||
                        !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                          (compoundend && !words && TESTAFF(rv->astr, compoundend, rv->alen)) ||
                           (numdefcpd && words && defcpd_check(&words, wnum + 1, rv, NULL,1))))) {
            rv = rv->next_homonym;
        }

            if (rv && words && words[wnum + 1]) return rv;

            oldnumsyllable2 = numsyllable;
            oldwordnum2 = wordnum;

// LANG_hu section: spec. Hungarian rule, XXX hardwired dictionary code
            if ((rv) && (langnum == LANG_hu) && (TESTAFF(rv->astr, 'I', rv->alen)) && !(TESTAFF(rv->astr, 'J', rv->alen))) {
                numsyllable--;
            }
// END of LANG_hu section

            // increment word number, if the second root has a compoundroot flag
            if ((rv) && (compoundroot) && 
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                    wordnum++;
            }

            // check forbiddenwords
            if ((rv) && (rv->astr) && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
               (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen)))) return NULL;

            // second word is acceptable, as a root?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist of 2 words, or if more,
            // then the syllable number of root words must be 6, or lesser.

            if ((rv) && (
                      (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                      (compoundend && TESTAFF(rv->astr, compoundend, rv->alen))
                    )
                && (
                      ((cpdwordmax==-1) || (wordnum+1<cpdwordmax)) || 
                      ((cpdmaxsyllable==0) || 
                          (numsyllable + get_syllable(&(rv->word), rv->clen)<=cpdmaxsyllable))
                    )
                && (
                     (!checkcompounddup || (rv != rv_first))
                   )
                )
                 {
                      // forbid compound word, if it is a non compound word with typical fault
                      if (checkcompoundrep && cpdrep_check(word,len)) return NULL;
                      return rv;
            }

            numsyllable = oldnumsyllable2 ;
            wordnum = oldwordnum2;

            // perhaps second word has prefix or/and suffix
            sfx = NULL;
            sfxflag = FLAG_NULL;
            rv = (compoundflag) ? affix_check((word+i),strlen(word+i), compoundflag, IN_CPD_END) : NULL;
            if (!rv && compoundend) {
                sfx = NULL;
                pfx = NULL;
                rv = affix_check((word+i),strlen(word+i), compoundend, IN_CPD_END);
            }
            
            if (!rv && numdefcpd && words) {
                rv = affix_check((word+i),strlen(word+i), 0, IN_CPD_END);
                if (rv && defcpd_check(&words, wnum + 1, rv, NULL, 1)) return rv;
                rv = NULL;
            }

            // check non_compound flag in suffix and prefix
            if ((rv) && 
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundforbidflag, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }

            // check forbiddenwords
            if ((rv) && (rv->astr) && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
               (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen)))) return NULL;

            // pfxappnd = prefix of word+i, or NULL
            // calculate syllable number of prefix.
            // hungarian convention: when syllable number of prefix is more,
            // than 1, the prefix+word counts as two words.

            if (langnum == LANG_hu) {
                // calculate syllable number of the word
                numsyllable += get_syllable(word + i, strlen(word + i));
                
                // - affix syllable num.
                // XXX only second suffix (inflections, not derivations)
                if (sfxappnd) {
                    char * tmp = myrevstrdup(sfxappnd);
                    numsyllable -= get_syllable(tmp, strlen(tmp));
                    free(tmp);
                }
                
                // + 1 word, if syllable number of the prefix > 1 (hungarian convention)
                if (pfx && (get_syllable(((PfxEntry *)pfx)->getKey(),strlen(((PfxEntry *)pfx)->getKey())) > 1)) wordnum++;

                // increment syllable num, if last word has a SYLLABLENUM flag
                // and the suffix is beginning `s'
            
                if (cpdsyllablenum) {
                    switch (sfxflag) {
                        case 'c': { numsyllable+=2; break; }
                        case 'J': { numsyllable += 1; break; }
                        case 'I': { if (TESTAFF(rv->astr, 'J', rv->alen)) numsyllable += 1; break; }
                    }
                }
            }
            
            // increment word number, if the second word has a compoundroot flag
            if ((rv) && (compoundroot) && 
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                    wordnum++;
            }

            // second word is acceptable, as a word with prefix or/and suffix?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist 2 word, otherwise
            // the syllable number of root words is 6, or lesser.
            if ((rv) && 
                    (
                      ((cpdwordmax == -1) || (wordnum + 1 < cpdwordmax)) || 
                      ((cpdmaxsyllable == 0) || 
                          (numsyllable <= cpdmaxsyllable))
                    )
                && (
                   (!checkcompounddup || (rv != rv_first))
                   )) {
                    // forbid compound word, if it is a non compound word with typical fault
                    if (checkcompoundrep && cpdrep_check(word, len)) return NULL;
                    return rv;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;
#ifdef HUNSTEM
            if (cmpdstemnum) oldcmpdstemnum = *cmpdstemnum;
#endif
            // perhaps second word is a compound word (recursive call)
            if (wordnum < maxwordnum) {
                rv = compound_check((word+i),strlen(word+i), wordnum+1,
                     numsyllable, maxwordnum, wnum + 1, words,
                     0, cmpdstemnum, cmpdstem, is_sug);
            } else {
                rv=NULL;
            }
            if (rv) {
                // forbid compound word, if it is a non compound word with typical fault
                if (checkcompoundrep && cpdrep_check(word, len)) return NULL;
                return rv;
            } else {
#ifdef HUNSTEM
            if (cmpdstemnum) *cmpdstemnum = oldcmpdstemnum;
#endif
            }
        }
        st[i] = ch;
        wordnum = oldwordnum;
        numsyllable = oldnumsyllable;
    }
    
    return NULL;
}    

#ifdef HUNSPELL_EXPERIMENTAL
// check if compound word is correctly spelled
// hu_mov_rule = spec. Hungarian rule (XXX)
int AffixMgr::compound_check_morph(const char * word, int len, 
    short wordnum, short numsyllable, short maxwordnum, short wnum, hentry ** words,
    char hu_mov_rule = 0, char ** result = NULL, char * partresult = NULL)
{
    int i;
    short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
    int ok = 0;

    struct hentry * rv = NULL;
    struct hentry * rv_first;
    struct hentry * rwords[MAXWORDLEN]; // buffer for COMPOUND pattern checking
    char st [MAXWORDUTF8LEN + 4];
    char ch;
    
    int checked_prefix;
    char presult[MAXLNLEN];

    int cmin;
    int cmax;
    
    if (utf8) {
        for (cmin = 0, i = 0; (i < cpdmin) && word[cmin]; i++) {
          cmin++;
          for (; (word[cmin] & 0xc0) == 0x80; cmin++);
        }
        for (cmax = len, i = 0; (i < (cpdmin - 1)) && cmax; i++) {
          cmax--;
          for (; (word[cmax] & 0xc0) == 0x80; cmax--);
        }
    } else {
        cmin = cpdmin;
        cmax = len - cpdmin + 1;
    }

    strcpy(st, word);

    for (i = cmin; i < cmax; i++) {
        oldnumsyllable = numsyllable;
        oldwordnum = wordnum;
        checked_prefix = 0;

        // go to end of the UTF-8 character
        if (utf8) {
            for (; (st[i] & 0xc0) == 0x80; i++);
            if (i >= cmax) return 0;
        }
        
        ch = st[i];
        st[i] = '\0';
        sfx = NULL;

        // FIRST WORD
        *presult = '\0';
        if (partresult) strcat(presult, partresult);
        
        rv = lookup(st); // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && !hu_mov_rule && 
            ((pseudoroot && TESTAFF(rv->astr, pseudoroot, rv->alen)) ||
                !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                (compoundbegin && !wordnum &&
                        TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
                (compoundmiddle && wordnum && !words &&
                    TESTAFF(rv->astr, compoundmiddle, rv->alen)) ||
                  (numdefcpd &&
                    ((!words && !wordnum && defcpd_check(&words, wnum, rv, (hentry **) &rwords, 0)) ||
                    (words && defcpd_check(&words, wnum, rv, (hentry **) &rwords, 0))))
                  ))) {
            rv = rv->next_homonym;
        }

        if (rv)  {
            if (rv->description) {
                if ((!rv->astr) || !TESTAFF(rv->astr, lemma_present, rv->alen))
                                        strcat(presult, st);
                strcat(presult, rv->description);
            }
        }
        
        if (!rv) {
            if (compoundflag && 
             !(rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundflag))) {
                if ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL,
                        FLAG_NULL, compoundflag, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) && !hu_mov_rule &&
                    ((SfxEntry*)sfx)->getCont() &&
                        ((compoundforbidflag && TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                            ((SfxEntry*)sfx)->getContLen())) || (compoundend &&
                        TESTAFF(((SfxEntry*)sfx)->getCont(), compoundend, 
                            ((SfxEntry*)sfx)->getContLen())))) {
                        rv = NULL;
                }
            }
            
            if (rv ||
              (((wordnum == 0) && compoundbegin &&
                ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL, FLAG_NULL, compoundbegin, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                (rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundbegin)))) ||
              ((wordnum > 0) && compoundmiddle &&
                ((rv = suffix_check(st, i, 0, NULL, NULL, 0, NULL, FLAG_NULL, compoundmiddle, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                (rv = prefix_check(st, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, compoundmiddle)))))
              ) {
                //char * p = prefix_check_morph(st, i, 0, compound);
                char * p = NULL;
                if (compoundflag) p = affix_check_morph(st, i, compoundflag);
                if (!p || (*p == '\0')) {
                   if ((wordnum == 0) && compoundbegin) {
                     p = affix_check_morph(st, i, compoundbegin);
                   } else if ((wordnum > 0) && compoundmiddle) {
                     p = affix_check_morph(st, i, compoundmiddle);                   
                   }
                }
                if (*p != '\0') {
                    line_uniq(p);
                    if (strchr(p, '\n')) {
                        strcat(presult, "(");
                        strcat(presult, line_join(p, '|'));
                        strcat(presult, ")");
                      } else {
                        strcat(presult, p);
                      }
                }
                if (presult[strlen(presult) - 1] == '\n') {
                    presult[strlen(presult) - 1] = '\0';
                }
                checked_prefix = 1;
                //strcat(presult, "+");
            }
        // else check forbiddenwords
        } else if (rv->astr && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
            TESTAFF(rv->astr, pseudoroot, rv->alen))) {
                st[i] = ch;
                continue;
        }

            // check non_compound flag in suffix and prefix
            if ((rv) && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundforbidflag, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    continue;
            }

            // check compoundend flag in suffix and prefix
            if ((rv) && !checked_prefix && compoundend && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundend, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundend, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    continue;
            }

            // check compoundmiddle flag in suffix and prefix
            if ((rv) && !checked_prefix && (wordnum==0) && compoundmiddle && !hu_mov_rule &&
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundmiddle, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundmiddle, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }       

        // check forbiddenwords
        if ((rv) && (rv->astr) && TESTAFF(rv->astr, forbiddenword, rv->alen)) continue;

        // increment word number, if the second root has a compoundroot flag
        if ((rv) && (compoundroot) && 
            (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                wordnum++;
        }

        // first word is acceptable in compound words?
        if (((rv) && 
          ( checked_prefix || (words && words[wnum]) ||
            (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
            ((oldwordnum == 0) && compoundbegin && TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
            ((oldwordnum > 0) && compoundmiddle && TESTAFF(rv->astr, compoundmiddle, rv->alen)) 
// LANG_hu section: spec. Hungarian rule
            || ((langnum == LANG_hu) && // hu_mov_rule
                hu_mov_rule && (
                    TESTAFF(rv->astr, 'F', rv->alen) ||
                    TESTAFF(rv->astr, 'G', rv->alen) ||
                    TESTAFF(rv->astr, 'H', rv->alen)
                )
              )
// END of LANG_hu section
          )
          && ! (( checkcompoundtriple && // test triple letters
                   (word[i-1]==word[i]) && (
                      ((i>1) && (word[i-1]==word[i-2])) || 
                      ((word[i-1]==word[i+1])) // may be word[i+1] == '\0'
                   )
               ) ||
               (
                   // test CHECKCOMPOUNDPATTERN
                   numcheckcpd && cpdpat_check(word, i)
               ) ||
               ( 
                 checkcompoundcase && cpdcase_check(word, i)
               ))
         )
// LANG_hu section: spec. Hungarian rule
         || ((!rv) && (langnum == LANG_hu) && hu_mov_rule && (rv = affix_check(st,i)) &&
              (sfx && ((SfxEntry*)sfx)->getCont() && (
                        TESTAFF(((SfxEntry*)sfx)->getCont(), (unsigned short) 'x', ((SfxEntry*)sfx)->getContLen()) ||
                        TESTAFF(((SfxEntry*)sfx)->getCont(), (unsigned short) '%', ((SfxEntry*)sfx)->getContLen())
                    )                
               )
             )
// END of LANG_hu section
         ) {

// LANG_hu section: spec. Hungarian rule
            if (langnum == LANG_hu) {
                // calculate syllable number of the word
                numsyllable += get_syllable(st, i);

                // + 1 word, if syllable number of the prefix > 1 (hungarian convention)
                if (pfx && (get_syllable(((PfxEntry *)pfx)->getKey(),strlen(((PfxEntry *)pfx)->getKey())) > 1)) wordnum++;
            }
// END of LANG_hu section

            // NEXT WORD(S)
            rv_first = rv;
            rv = lookup((word+i)); // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && ((pseudoroot && TESTAFF(rv->astr, pseudoroot, rv->alen)) ||
                        !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                          (compoundend && !words && TESTAFF(rv->astr, compoundend, rv->alen)) ||
                           (numdefcpd && defcpd_check(&words, wnum + 1, rv, NULL,1))))) {
            rv = rv->next_homonym;
        }

            if (rv && words && words[wnum + 1]) {
                  strcat(*result, presult);
                  if (complexprefixes && rv->description) strcat(*result, rv->description);
                  if (rv->description && ((!rv->astr) || 
                     !TESTAFF(rv->astr, lemma_present, rv->alen)))
                        strcat(*result, &(rv->word));
                  if (!complexprefixes && rv->description) strcat(*result, rv->description);
                  strcat(*result, "\n");
                  ok = 1;
                  return 0;
            }

            oldnumsyllable2 = numsyllable;
            oldwordnum2 = wordnum;

// LANG_hu section: spec. Hungarian rule
            if ((rv) && (langnum == LANG_hu) && (TESTAFF(rv->astr, 'I', rv->alen)) && !(TESTAFF(rv->astr, 'J', rv->alen))) {
                numsyllable--;
            }
// END of LANG_hu section
            // increment word number, if the second root has a compoundroot flag
            if ((rv) && (compoundroot) && 
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                    wordnum++;
            }

            // check forbiddenwords
            if ((rv) && (rv->astr) && TESTAFF(rv->astr, forbiddenword, rv->alen)) {
                st[i] = ch;
                continue;
            }
                    
            // second word is acceptable, as a root?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist of 2 words, or if more,
            // then the syllable number of root words must be 6, or lesser.
            if ((rv) && (
                      (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                      (compoundend && TESTAFF(rv->astr, compoundend, rv->alen))
                    )
                && (
                      ((cpdwordmax==-1) || (wordnum+1<cpdwordmax)) || 
                      ((cpdmaxsyllable==0) || 
                          (numsyllable+get_syllable(&(rv->word),rv->wlen)<=cpdmaxsyllable))
                    )
                && (
                     (!checkcompounddup || (rv != rv_first))
                   )
                )
                 {
                      // bad compound word
                      strcat(*result, presult);
                                          
                      if (rv->description) {
                        if (complexprefixes) strcat(*result, rv->description);
                        if ((!rv->astr) || !TESTAFF(rv->astr, lemma_present, rv->alen))
                                               strcat(*result, &(rv->word));
                        if (!complexprefixes) strcat(*result, rv->description);
                      }
                      strcat(*result, "\n");
                              ok = 1;
            }

            numsyllable = oldnumsyllable2 ;
            wordnum = oldwordnum2;

            // perhaps second word has prefix or/and suffix
            sfx = NULL;
            sfxflag = FLAG_NULL;

            if (compoundflag) rv = affix_check((word+i),strlen(word+i), compoundflag); else rv = NULL;

            if (!rv && compoundend) {
                sfx = NULL;
                pfx = NULL;
                rv = affix_check((word+i),strlen(word+i), compoundend);
            }

            if (!rv && numdefcpd && words) {
                rv = affix_check((word+i),strlen(word+i), 0, IN_CPD_END);
                if (rv && words && defcpd_check(&words, wnum + 1, rv, NULL, 1)) {
                      char * m = NULL;
                      if (compoundflag) m = affix_check_morph((word+i),strlen(word+i), compoundflag);
                      if ((!m || *m == '\0') && compoundend)
                            m = affix_check_morph((word+i),strlen(word+i), compoundend);
                      strcat(*result, presult);
                      if (m) {
                        line_uniq(m);
                        if (strchr(m, '\n')) {
                            strcat(*result, "(");
                            strcat(*result, line_join(m, '|'));
                            strcat(*result, ")");
                        } else {
                            strcat(*result, m);
                        }
                        free(m);
                      }
                      strcat(*result, "\n");
                      ok = 1;
                }
            }

            // check non_compound flag in suffix and prefix
            if ((rv) && 
                ((pfx && ((PfxEntry*)pfx)->getCont() &&
                    TESTAFF(((PfxEntry*)pfx)->getCont(), compoundforbidflag, 
                        ((PfxEntry*)pfx)->getContLen())) ||
                (sfx && ((SfxEntry*)sfx)->getCont() &&
                    TESTAFF(((SfxEntry*)sfx)->getCont(), compoundforbidflag, 
                        ((SfxEntry*)sfx)->getContLen())))) {
                    rv = NULL;
            }

            // check forbiddenwords
            if ((rv) && (rv->astr) && (TESTAFF(rv->astr,forbiddenword,rv->alen))
                    && (! TESTAFF(rv->astr, pseudoroot, rv->alen))) {
                        st[i] = ch;
                        continue;
                    }

            if (langnum == LANG_hu) {
                // calculate syllable number of the word
                numsyllable += get_syllable(word + i, strlen(word + i));

                // - affix syllable num.
                // XXX only second suffix (inflections, not derivations)
                if (sfxappnd) {
                    char * tmp = myrevstrdup(sfxappnd);
                    numsyllable -= get_syllable(tmp, strlen(tmp));
                    free(tmp);
                }

                // + 1 word, if syllable number of the prefix > 1 (hungarian convention)
                if (pfx && (get_syllable(((PfxEntry *)pfx)->getKey(),strlen(((PfxEntry *)pfx)->getKey())) > 1)) wordnum++;

                // increment syllable num, if last word has a SYLLABLENUM flag
                // and the suffix is beginning `s'

                if (cpdsyllablenum) {
                    switch (sfxflag) {
                        case 'c': { numsyllable+=2; break; }
                        case 'J': { numsyllable += 1; break; }
                        case 'I': { if (rv && TESTAFF(rv->astr, 'J', rv->alen)) numsyllable += 1; break; }
                    }
                }
            }

            // increment word number, if the second word has a compoundroot flag
            if ((rv) && (compoundroot) && 
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
                    wordnum++;
            }
            // second word is acceptable, as a word with prefix or/and suffix?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist 2 word, otherwise
            // the syllable number of root words is 6, or lesser.
            if ((rv) && 
                    (
                      ((cpdwordmax==-1) || (wordnum+1<cpdwordmax)) || 
                      ((cpdmaxsyllable==0) || 
                          (numsyllable <= cpdmaxsyllable))
                    )
                && (
                   (!checkcompounddup || (rv != rv_first))
                   )) {
                      char * m = NULL;
                      if (compoundflag) m = affix_check_morph((word+i),strlen(word+i), compoundflag);
                      if ((!m || *m == '\0') && compoundend)
                            m = affix_check_morph((word+i),strlen(word+i), compoundend);
                      strcat(*result, presult);
                      if (m) {
                        line_uniq(m);
                        if (strchr(m, '\n')) {
                            strcat(*result, "(");
                            strcat(*result, line_join(m, '|'));
                            strcat(*result, ")");
                        } else {
                            strcat(*result, m);
                        }
                        free(m);
                      }
                      strcat(*result, "\n");
                      ok = 1;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;

            // perhaps second word is a compound word (recursive call)
            if ((wordnum < maxwordnum) && (ok == 0)) {
                        compound_check_morph((word+i),strlen(word+i), wordnum+1, 
                             numsyllable, maxwordnum, wnum + 1, words, 0, result, presult);
            } else {
                rv=NULL;
            }
        }
        st[i] = ch;
        wordnum = oldwordnum;
        numsyllable = oldnumsyllable;
    }
    return 0;
}    
#endif // END OF HUNSPELL_EXPERIMENTAL CODE

 // return 1 if s1 (reversed) is a leading subset of end of s2
/* inline int AffixMgr::isRevSubset(const char * s1, const char * end_of_s2, int len)
 {
    while ((len > 0) && *s1 && (*s1 == *end_of_s2)) {
        s1++;
        end_of_s2--;
        len--;
    }
    return (*s1 == '\0');
 }
 */

inline int AffixMgr::isRevSubset(const char * s1, const char * end_of_s2, int len)
 {
    while ((len > 0) && (*s1 != '\0') && ((*s1 == *end_of_s2) || (*s1 == '.'))) {
        s1++;
        end_of_s2--;
        len--;
    }
    return (*s1 == '\0');
 }

// check word for suffixes

struct hentry * AffixMgr::suffix_check (const char * word, int len, 
       int sfxopts, AffEntry * ppfx, char ** wlst, int maxSug, int * ns, 
       const FLAG cclass, const FLAG needflag, char in_compound)
{
    struct hentry * rv = NULL;
    char result[MAXLNLEN];

    PfxEntry* ep = (PfxEntry *) ppfx;

    // first handle the special case of 0 length suffixes
    SfxEntry * se = (SfxEntry *) sStart[0];

    while (se) {
        if (!cclass || se->getCont()) {
            // suffixes are not allowed in beginning of compounds
            if ((((in_compound != IN_CPD_BEGIN)) || // && !cclass
             // except when signed with compoundpermitflag flag
             (se->getCont() && compoundpermitflag &&
                TESTAFF(se->getCont(),compoundpermitflag,se->getContLen()))) && (!circumfix ||
              // no circumfix flag in prefix and suffix
              ((!ppfx || !(ep->getCont()) || !TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (!se->getCont() || !(TESTAFF(se->getCont(),circumfix,se->getContLen())))) ||
              // circumfix flag in prefix AND suffix
              ((ppfx && (ep->getCont()) && TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (se->getCont() && (TESTAFF(se->getCont(),circumfix,se->getContLen())))))  &&
            // fogemorpheme
              (in_compound || 
                 !((se->getCont() && (TESTAFF(se->getCont(), onlyincompound, se->getContLen()))))) &&
            // pseudoroot on prefix or first suffix
              (cclass || 
                   !(se->getCont() && TESTAFF(se->getCont(), pseudoroot, se->getContLen())) ||
                   (ppfx && !((ep->getCont()) &&
                     TESTAFF(ep->getCont(), pseudoroot,
                       ep->getContLen())))
              )
            ) {
                rv = se->checkword(word,len, sfxopts, ppfx, wlst, maxSug, ns, (FLAG) cclass, 
                    needflag, (in_compound ? 0 : onlyincompound));
                if (rv) {
                    sfx=(AffEntry *)se; // BUG: sfx not stateless
                    return rv;
                }
            }
        }
       se = se->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)(word + len - 1));
    SfxEntry * sptr = (SfxEntry *) sStart[sp];

    while (sptr) {
        if (isRevSubset(sptr->getKey(), word + len - 1, len)
        ) {
            // suffixes are not allowed in beginning of compounds
            if ((((in_compound != IN_CPD_BEGIN)) || // && !cclass
             // except when signed with compoundpermitflag flag
             (sptr->getCont() && compoundpermitflag &&
                TESTAFF(sptr->getCont(),compoundpermitflag,sptr->getContLen()))) && (!circumfix ||
              // no circumfix flag in prefix and suffix
              ((!ppfx || !(ep->getCont()) || !TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (!sptr->getCont() || !(TESTAFF(sptr->getCont(),circumfix,sptr->getContLen())))) ||
              // circumfix flag in prefix AND suffix
              ((ppfx && (ep->getCont()) && TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (sptr->getCont() && (TESTAFF(sptr->getCont(),circumfix,sptr->getContLen())))))  &&
            // fogemorpheme
              (in_compound || 
                 !((sptr->getCont() && (TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))))) &&
            // pseudoroot on prefix or first suffix
              (cclass || 
                  !(sptr->getCont() && TESTAFF(sptr->getCont(), pseudoroot, sptr->getContLen())) ||
                  (ppfx && !((ep->getCont()) &&
                     TESTAFF(ep->getCont(), pseudoroot,
                       ep->getContLen())))
              )
            ) {
                rv = sptr->checkword(word,len, sfxopts, ppfx, wlst,
                    maxSug, ns, cclass, needflag, (in_compound ? 0 : onlyincompound));
                if (rv) {
                    sfx=(AffEntry *)sptr; // BUG: sfx not stateless
                    sfxflag = sptr->getFlag(); // BUG: sfxflag not stateless
                    if (!sptr->getCont()) sfxappnd=sptr->getKey(); // BUG: sfxappnd not stateless
                    if (cclass || sptr->getCont()) {
                                if (!derived) {
                                        derived = mystrdup(word);
                                } else {
                                        strcpy(result, derived); // XXX check size
                                        strcat(result, "\n");
                                        strcat(result, word);
                                        free(derived);
                                        derived = mystrdup(result);
                                }
                    }
                    return rv;
                }
             }
             sptr = sptr->getNextEQ();
        } else {
             sptr = sptr->getNextNE();
        }
    }

    return NULL;
}

// check word for two-level suffixes

struct hentry * AffixMgr::suffix_check_twosfx(const char * word, int len, 
       int sfxopts, AffEntry * ppfx, const FLAG needflag)
{
    struct hentry * rv = NULL;

    // first handle the special case of 0 length suffixes
    SfxEntry * se = (SfxEntry *) sStart[0];
    while (se) {
        if (contclasses[se->getFlag()])
        {
            rv = se->check_twosfx(word,len, sfxopts, ppfx, needflag);
            if (rv) return rv;
        }
        se = se->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)(word + len - 1));
    SfxEntry * sptr = (SfxEntry *) sStart[sp];

    while (sptr) {
        if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
            if (contclasses[sptr->getFlag()])
            {
                rv = sptr->check_twosfx(word,len, sfxopts, ppfx, needflag);
                if (rv) {
                    sfxflag = sptr->getFlag(); // BUG: sfxflag not stateless
                    if (!sptr->getCont()) sfxappnd=sptr->getKey(); // BUG: sfxappnd not stateless
                    return rv;
                }
            }
            sptr = sptr->getNextEQ();
        } else {
             sptr = sptr->getNextNE();
        }
    }

    return NULL;
}

#ifdef HUNSPELL_EXPERIMENTAL
char * AffixMgr::suffix_check_twosfx_morph(const char * word, int len, 
       int sfxopts, AffEntry * ppfx, const FLAG needflag)
{
    char result[MAXLNLEN];
    char result2[MAXLNLEN];
    char result3[MAXLNLEN];
    
    char * st;

    result[0] = '\0';
    result2[0] = '\0';
    result3[0] = '\0';

    // first handle the special case of 0 length suffixes
    SfxEntry * se = (SfxEntry *) sStart[0];
    while (se) {
        if (contclasses[se->getFlag()])
        {
            st = se->check_twosfx_morph(word,len, sfxopts, ppfx, needflag);
            if (st) {
                if (ppfx) {
                    if (((PfxEntry *) ppfx)->getMorph()) strcat(result, ((PfxEntry *) ppfx)->getMorph());
                }
                strcat(result, st);
                free(st);
                if (se->getMorph()) strcat(result, se->getMorph());
                strcat(result, "\n");
            }
        }
        se = se->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)(word + len - 1));
    SfxEntry * sptr = (SfxEntry *) sStart[sp];

    while (sptr) {
        if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
            if (contclasses[sptr->getFlag()]) 
            {
                st = sptr->check_twosfx_morph(word,len, sfxopts, ppfx, needflag);
                if (st) {
                    sfxflag = sptr->getFlag(); // BUG: sfxflag not stateless
                    if (!sptr->getCont()) sfxappnd=sptr->getKey(); // BUG: sfxappnd not stateless
                    strcpy(result2, st);
                    free(st);

                result3[0] = '\0';
#ifdef DEBUG
                unsigned short flag = sptr->getFlag();
                if (flag_mode == FLAG_NUM) {
                    sprintf(result3, "<%d>", sptr->getKey());
                } else if (flag_mode == FLAG_LONG) {
                    sprintf(result3, "<%c%c>", flag >> 8, (flag << 8) >>8);
                } else sprintf(result3, "<%c>", flag);
                strcat(result3, ":");
#endif
                if (sptr->getMorph()) strcat(result3, sptr->getMorph());
                strlinecat(result2, result3);
                strcat(result2, "\n");
                strcat(result,  result2);
                }
            }
            sptr = sptr->getNextEQ();
        } else {
             sptr = sptr->getNextNE();
        }
    }
    if (result) return mystrdup(result);
    return NULL;
}

char * AffixMgr::suffix_check_morph(const char * word, int len, 
       int sfxopts, AffEntry * ppfx, const FLAG cclass, const FLAG needflag, char in_compound)
{
    char result[MAXLNLEN];
    
    struct hentry * rv = NULL;

    result[0] = '\0';

    PfxEntry* ep = (PfxEntry *) ppfx;

    // first handle the special case of 0 length suffixes
    SfxEntry * se = (SfxEntry *) sStart[0];
    while (se) {
        if (!cclass || se->getCont()) {
            // suffixes are not allowed in beginning of compounds
            if (((((in_compound != IN_CPD_BEGIN)) || // && !cclass
             // except when signed with compoundpermitflag flag
             (se->getCont() && compoundpermitflag &&
                TESTAFF(se->getCont(),compoundpermitflag,se->getContLen()))) && (!circumfix ||
              // no circumfix flag in prefix and suffix
              ((!ppfx || !(ep->getCont()) || !TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (!se->getCont() || !(TESTAFF(se->getCont(),circumfix,se->getContLen())))) ||
              // circumfix flag in prefix AND suffix
              ((ppfx && (ep->getCont()) && TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (se->getCont() && (TESTAFF(se->getCont(),circumfix,se->getContLen())))))  &&
            // fogemorpheme
              (in_compound || 
                 !((se->getCont() && (TESTAFF(se->getCont(), onlyincompound, se->getContLen()))))) &&
            // pseudoroot on prefix or first suffix
              (cclass || 
                   !(se->getCont() && TESTAFF(se->getCont(), pseudoroot, se->getContLen())) ||
                   (ppfx && !((ep->getCont()) &&
                     TESTAFF(ep->getCont(), pseudoroot,
                       ep->getContLen())))
              )
            ))
            rv = se->checkword(word,len, sfxopts, ppfx, NULL, 0, 0, cclass, needflag);
         while (rv) {
           if (ppfx) {
                if (((PfxEntry *) ppfx)->getMorph()) strcat(result, ((PfxEntry *) ppfx)->getMorph());
            }
            if (complexprefixes && rv->description) strcat(result, rv->description);
            if (rv->description && ((!rv->astr) || 
                                        !TESTAFF(rv->astr, lemma_present, rv->alen)))
                                               strcat(result, &(rv->word));
            if (!complexprefixes && rv->description) strcat(result, rv->description);
            if (se->getMorph()) strcat(result, se->getMorph());
            strcat(result, "\n");
            rv = se->get_next_homonym(rv, sfxopts, ppfx, cclass, needflag);
         }
       }
       se = se->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)(word + len - 1));
    SfxEntry * sptr = (SfxEntry *) sStart[sp];

    while (sptr) {
        if (isRevSubset(sptr->getKey(), word + len - 1, len)
        ) {
            // suffixes are not allowed in beginning of compounds
            if (((((in_compound != IN_CPD_BEGIN)) || // && !cclass
             // except when signed with compoundpermitflag flag
             (sptr->getCont() && compoundpermitflag &&
                TESTAFF(sptr->getCont(),compoundpermitflag,sptr->getContLen()))) && (!circumfix ||
              // no circumfix flag in prefix and suffix
              ((!ppfx || !(ep->getCont()) || !TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (!sptr->getCont() || !(TESTAFF(sptr->getCont(),circumfix,sptr->getContLen())))) ||
              // circumfix flag in prefix AND suffix
              ((ppfx && (ep->getCont()) && TESTAFF(ep->getCont(),
                   circumfix, ep->getContLen())) &&
               (sptr->getCont() && (TESTAFF(sptr->getCont(),circumfix,sptr->getContLen())))))  &&
            // fogemorpheme
              (in_compound || 
                 !((sptr->getCont() && (TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))))) &&
            // pseudoroot on first suffix
              (cclass || !(sptr->getCont() && 
                   TESTAFF(sptr->getCont(), pseudoroot, sptr->getContLen())))
            )) rv = sptr->checkword(word,len, sfxopts, ppfx, NULL, 0, 0, cclass, needflag);
            while (rv) {
                    if (ppfx) {
                        if (((PfxEntry *) ppfx)->getMorph()) strcat(result, ((PfxEntry *) ppfx)->getMorph());
                    }    
                    if (complexprefixes && rv->description) strcat(result, rv->description);
                    if (rv->description && ((!rv->astr) || 
                        !TESTAFF(rv->astr, lemma_present, rv->alen))) strcat(result, &(rv->word));
                    if (!complexprefixes && rv->description) strcat(result, rv->description);
#ifdef DEBUG
                unsigned short flag = sptr->getFlag();
                if (flag_mode == FLAG_NUM) {
                    sprintf(result, "<%d>", sptr->getKey());
                } else if (flag_mode == FLAG_LONG) {
                    sprintf(result, "<%c%c>", flag >> 8, (flag << 8) >>8);
                } else sprintf(result, "<%c>", flag);
                strcat(result, ":");
#endif

                if (sptr->getMorph()) strcat(result, sptr->getMorph());
                strcat(result, "\n");
                rv = sptr->get_next_homonym(rv, sfxopts, ppfx, cclass, needflag);
            }
             sptr = sptr->getNextEQ();
        } else {
             sptr = sptr->getNextNE();
        }
    }

    if (*result) return mystrdup(result);
    return NULL;
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE


// check if word with affixes is correctly spelled
struct hentry * AffixMgr::affix_check (const char * word, int len, const FLAG needflag, char in_compound)
{
    struct hentry * rv= NULL;
    if (derived) free(derived);
    derived =  NULL;

    // check all prefixes (also crossed with suffixes if allowed) 
    rv = prefix_check(word, len, in_compound, needflag);
    if (rv) return rv;

    // if still not found check all suffixes
    rv = suffix_check(word, len, 0, NULL, NULL, 0, NULL, FLAG_NULL, needflag, in_compound);

    if (havecontclass) {
        sfx = NULL;
        pfx = NULL;
        if (rv) return rv;
        // if still not found check all two-level suffixes
        rv = suffix_check_twosfx(word, len, 0, NULL, needflag);
        if (rv) return rv;
        // if still not found check all two-level suffixes
        rv = prefix_check_twosfx(word, len, IN_CPD_NOT, needflag);
    }
    return rv;
}

#ifdef HUNSPELL_EXPERIMENTAL
// check if word with affixes is correctly spelled
char * AffixMgr::affix_check_morph(const char * word, int len, const FLAG needflag, char in_compound)
{
    char result[MAXLNLEN];
    char * st = NULL;

    *result = '\0';
    
    // check all prefixes (also crossed with suffixes if allowed) 
    st = prefix_check_morph(word, len, in_compound);
    if (st) {
        strcat(result, st);
        free(st);
    }

    // if still not found check all suffixes    
    st = suffix_check_morph(word, len, 0, NULL, '\0', needflag, in_compound);
    if (st) {
        strcat(result, st);
        free(st);
    }

    if (havecontclass) {
        sfx = NULL;
        pfx = NULL;
        // if still not found check all two-level suffixes
        st = suffix_check_twosfx_morph(word, len, 0, NULL, needflag);
        if (st) {
            strcat(result, st);
            free(st);
        }

        // if still not found check all two-level suffixes
        st = prefix_check_twosfx_morph(word, len, IN_CPD_NOT, needflag);
        if (st) {
            strcat(result, st);
            free(st);
        }
    }
    
    return mystrdup(result);
}
#endif // END OF HUNSPELL_EXPERIMENTAL CODE


int AffixMgr::expand_rootword(struct guessword * wlst, int maxn, const char * ts,
    int wl, const unsigned short * ap, unsigned short al, char * bad, int badl,
    char * phone)
{

    int nh=0;
    // first add root word to list
    if ((nh < maxn) && !(al && ((pseudoroot && TESTAFF(ap, pseudoroot, al)) ||
         (onlyincompound && TESTAFF(ap, onlyincompound, al))))) {
       wlst[nh].word = mystrdup(ts);
       wlst[nh].allow = (1 == 0);
       wlst[nh].orig = NULL;
       nh++;
       // add special phonetic version
       if (phone && (nh < maxn)) {
    	    wlst[nh].word = mystrdup(phone);
    	    wlst[nh].allow = (1 == 0);
    	    wlst[nh].orig = mystrdup(ts);
    	    nh++;
       }
    }

    // handle suffixes
    for (int i = 0; i < al; i++) {
       const unsigned char c = (unsigned char) (ap[i] & 0x00FF);
       SfxEntry * sptr = (SfxEntry *)sFlag[c];
       while (sptr) {
         if ((sptr->getFlag() == ap[i]) && (!sptr->getKeyLen() || ((badl > sptr->getKeyLen()) &&
                (strcmp(sptr->getAffix(), bad + badl - sptr->getKeyLen()) == 0))) &&
                // check pseudoroot flag
                !(sptr->getCont() && ((pseudoroot && 
                      TESTAFF(sptr->getCont(), pseudoroot, sptr->getContLen())) ||
                  (circumfix && 
                      TESTAFF(sptr->getCont(), circumfix, sptr->getContLen())) ||
                  (onlyincompound && 
                      TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))))
                ) {
            char * newword = sptr->add(ts, wl);
            if (newword) {
                if (nh < maxn) {
                    wlst[nh].word = newword;
                    wlst[nh].allow = sptr->allowCross();
                    wlst[nh].orig = NULL;
                    nh++;
                    // add special phonetic version
    		    if (phone && (nh < maxn)) {
    			char st[MAXWORDUTF8LEN];
    			strcpy(st, phone);
    			strcat(st, sptr->getKey());
    			reverseword(st + strlen(phone));
    			wlst[nh].word = mystrdup(st);
    			wlst[nh].allow = (1 == 0);
    			wlst[nh].orig = mystrdup(newword);
    			nh++;
    		    }
                } else {
                    free(newword);
                }
            }
         }
         sptr = (SfxEntry *)sptr ->getFlgNxt();
       }
    }

    int n = nh;

    // handle cross products of prefixes and suffixes
    for (int j=1;j<n ;j++)
       if (wlst[j].allow) {
          for (int k = 0; k < al; k++) {
    	     const unsigned char c = (unsigned char) (ap[k] & 0x00FF);
             PfxEntry * cptr = (PfxEntry *) pFlag[c];
             while (cptr) {
                if ((cptr->getFlag() == ap[k]) && cptr->allowCross() && (!cptr->getKeyLen() || ((badl > cptr->getKeyLen()) &&
                        (strncmp(cptr->getKey(), bad, cptr->getKeyLen()) == 0)))) {
                    int l1 = strlen(wlst[j].word);
                    char * newword = cptr->add(wlst[j].word, l1);
                    if (newword) {
                       if (nh < maxn) {
                          wlst[nh].word = newword;
                          wlst[nh].allow = cptr->allowCross();
                	  wlst[nh].orig = NULL;
                          nh++;
                       } else {
                          free(newword);
                       }
                    }
                }
                cptr = (PfxEntry *)cptr ->getFlgNxt();
             }
          }
       }


    // now handle pure prefixes
    for (int m = 0; m < al; m ++) {
       const unsigned char c = (unsigned char) (ap[m] & 0x00FF);
       PfxEntry * ptr = (PfxEntry *) pFlag[c];
       while (ptr) {
         if ((ptr->getFlag() == ap[m]) && (!ptr->getKeyLen() || ((badl > ptr->getKeyLen()) &&
                (strncmp(ptr->getKey(), bad, ptr->getKeyLen()) == 0))) &&
                // check pseudoroot flag
                !(ptr->getCont() && ((pseudoroot && 
                      TESTAFF(ptr->getCont(), pseudoroot, ptr->getContLen())) ||
                     (circumfix && 
                      TESTAFF(ptr->getCont(), circumfix, ptr->getContLen())) ||                      
                  (onlyincompound && 
                      TESTAFF(ptr->getCont(), onlyincompound, ptr->getContLen()))))
                ) {
            char * newword = ptr->add(ts, wl);
            if (newword) {
                if (nh < maxn) {
                    wlst[nh].word = newword;
                    wlst[nh].allow = ptr->allowCross();
                    wlst[nh].orig = NULL;
                    nh++;
                } else {
                    free(newword);
                } 
            }
         }
         ptr = (PfxEntry *)ptr ->getFlgNxt();
       }
    }

    return nh;
}



// return length of replacing table
int AffixMgr::get_numrep()
{
  return numrep;
}

// return replacing table
struct replentry * AffixMgr::get_reptable()
{
  if (! reptable ) return NULL;
  return reptable;
}

// return replacing table
struct phonetable * AffixMgr::get_phonetable()
{
  if (! phone ) return NULL;
  return phone;
}

// return length of character map table
int AffixMgr::get_nummap()
{
  return nummap;
}

// return character map table
struct mapentry * AffixMgr::get_maptable()
{
  if (! maptable ) return NULL;
  return maptable;
}

// return length of word break table
int AffixMgr::get_numbreak()
{
  return numbreak;
}

// return character map table
char ** AffixMgr::get_breaktable()
{
  if (! breaktable ) return NULL;
  return breaktable;
}

// return text encoding of dictionary
char * AffixMgr::get_encoding()
{
  if (! encoding ) {
      encoding = mystrdup("ISO8859-1");
  }
  return mystrdup(encoding);
}

// return text encoding of dictionary
int AffixMgr::get_langnum()
{
  return langnum;
}

// return double prefix option
int AffixMgr::get_complexprefixes()
{
  return complexprefixes;
}

FLAG AffixMgr::get_keepcase()
{
  return keepcase;
}

int AffixMgr::get_checksharps()
{
  return checksharps;
}

// return the preferred ignore string for suggestions
char * AffixMgr::get_ignore()
{
  if (!ignorechars) return NULL;
  return ignorechars;
}

// return the preferred ignore string for suggestions
unsigned short * AffixMgr::get_ignore_utf16(int * len)
{
  *len = ignorechars_utf16_len;
  return ignorechars_utf16;
}

// return the keyboard string for suggestions
char * AffixMgr::get_key_string()
{
  if (! keystring ) return NULL;
  return mystrdup(keystring);
}

// return the preferred try string for suggestions
char * AffixMgr::get_try_string()
{
  if (! trystring ) return NULL;
  return mystrdup(trystring);
}

// return the preferred try string for suggestions
const char * AffixMgr::get_wordchars()
{
  return wordchars;
}

unsigned short * AffixMgr::get_wordchars_utf16(int * len)
{
  *len = wordchars_utf16_len;
  return wordchars_utf16;
}

// is there compounding?
int AffixMgr::get_compound()
{
  return compoundflag || compoundbegin || numdefcpd;
}

// return the compound words control flag
FLAG AffixMgr::get_compoundflag()
{
  return compoundflag;
}

// return the forbidden words control flag
FLAG AffixMgr::get_forbiddenword()
{
  return forbiddenword;
}

// return the forbidden words control flag
FLAG AffixMgr::get_nosuggest()
{
  return nosuggest;
}

// return the forbidden words flag modify flag
FLAG AffixMgr::get_pseudoroot()
{
  return pseudoroot;
}

// return the onlyincompound flag
FLAG AffixMgr::get_onlyincompound()
{
  return onlyincompound;
}

// return the compound word signal flag
FLAG AffixMgr::get_compoundroot()
{
  return compoundroot;
}

// return the compound begin signal flag
FLAG AffixMgr::get_compoundbegin()
{
  return compoundbegin;
}

// return the value of checknum
int AffixMgr::get_checknum()
{
  return checknum;
}

// return the value of prefix
const char * AffixMgr::get_prefix()
{
  if (pfx) return ((PfxEntry *)pfx)->getKey();
  return NULL;
}

// return the value of suffix
const char * AffixMgr::get_suffix()
{
  return sfxappnd;
}

// return the value of derived form (base word with first suffix).
const char * AffixMgr::get_derived()
{
  return derived;
}

// return the value of suffix
const char * AffixMgr::get_version()
{
  return version;
}

// return lemma_present flag
FLAG AffixMgr::get_lemma_present()
{
  return lemma_present;
}

// utility method to look up root words in hash table
struct hentry * AffixMgr::lookup(const char * word)
{
  if (! pHMgr) return NULL;
  return pHMgr->lookup(word);
}

// return the value of suffix
const int AffixMgr::have_contclass()
{
  return havecontclass;
}

// return utf8
int AffixMgr::get_utf8()
{
  return utf8;
}

// return nosplitsugs
int AffixMgr::get_maxngramsugs(void)
{
  return maxngramsugs;
}

// return nosplitsugs
int AffixMgr::get_nosplitsugs(void)
{
  return nosplitsugs;
}

// return sugswithdots
int AffixMgr::get_sugswithdots(void)
{
  return sugswithdots;
}

/* parse flag */
int AffixMgr::parse_flag(char * line, unsigned short * out, const char * name) {
   char * s = NULL;
   if (*out != FLAG_NULL) {
      HUNSPELL_WARNING(stderr, "error: duplicate %s line\n", name);
      return 1;
   }
   if (parse_string(line, &s, name)) return 1;
   *out = pHMgr->decode_flag(s);
   free(s);
   return 0;
}

/* parse num */
int AffixMgr::parse_num(char * line, int * out, const char * name) {
   char * s = NULL;
   if (*out != -1) {
      HUNSPELL_WARNING(stderr, "error: duplicate %s line\n", name);
      return 1;
   }
   if (parse_string(line, &s, name)) return 1;
   *out = atoi(s);
   free(s);
   return 0;
}

/* parse in the max syllablecount of compound words and  */
int  AffixMgr::parse_cpdsyllable(char * line)
{
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   w_char w[MAXWORDLEN];
   piece = mystrsep(&tp, 0);
   while (piece) {
      if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { cpdmaxsyllable = atoi(piece); np++; break; }
             case 2: {
                if (!utf8) {
                    cpdvowels = mystrdup(piece);
                } else {
                    int n = u8_u16(w, MAXWORDLEN, piece);
                    if (n > 0) {
                        flag_qsort((unsigned short *) w, 0, n);
                        cpdvowels_utf16 = (w_char *) malloc(n * sizeof(w_char));
                        if (!cpdvowels_utf16) return 1;
                        memcpy(cpdvowels_utf16, w, n * sizeof(w_char));
                    }
                    cpdvowels_utf16_len = n;
                }
                np++;
                break;
             }
             default: break;
          }
          i++;
      }
      free(piece);
      piece = mystrsep(&tp, 0);
   }
   if (np < 2) {
      HUNSPELL_WARNING(stderr, "error: missing compoundsyllable information\n");
      return 1;
   }
   if (np == 2) cpdvowels = mystrdup("aeiouAEIOU");
   return 0;
}

/* parse in the typical fault correcting table */
int  AffixMgr::parse_reptable(char * line, FILE * af)
{
   if (numrep != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate REP tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numrep = atoi(piece);
                       if (numrep < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in replacement table\n");
                          free(piece);
                          return 1;
                       }
                       reptable = (replentry *) malloc(numrep * sizeof(struct replentry));
                       if (!reptable) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing replacement table information\n");
      return 1;
   } 
 
   /* now parse the numrep lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numrep; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        reptable[j].pattern = NULL;
        reptable[j].pattern2 = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"REP",3) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: replacement table is corrupt\n");
                                 numrep = 0;
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: { reptable[j].pattern = mystrrep(mystrdup(piece),"_"," "); break; }
                  case 2: { reptable[j].pattern2 = mystrrep(mystrdup(piece),"_"," "); break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if ((!(reptable[j].pattern)) || (!(reptable[j].pattern2))) {
             HUNSPELL_WARNING(stderr, "error: replacement table is corrupt\n");
             numrep = 0;
             return 1;
        }
   }
   return 0;
}

/* parse in the typical fault correcting table */
int  AffixMgr::parse_phonetable(char * line, FILE * af)
{
   if (phone) {
      HUNSPELL_WARNING(stderr, "error: duplicate PHONE tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
            	       phone = (phonetable *) malloc(sizeof(struct phonetable));
            	       phone->num = atoi(piece);
            	       phone->rules = NULL;
            	       phone->utf8 = utf8;
                       if (!phone) return 1;
                       if (phone->num < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in phonelacement table\n");
                          free(piece);
                          return 1;
                       }
                       phone->rules = (char * *) malloc(2 * (phone->num + 1) * sizeof(char *));
                       if (!phone->rules) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing PHONE table information\n");
      return 1;
   } 
 
   /* now parse the phone->num lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < phone->num; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        phone->rules[j * 2] = NULL;
        phone->rules[j * 2 + 1] = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"PHONE",5) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: PHONE table is corrupt\n");
                                 phone->num = 0;
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: { phone->rules[j * 2] = mystrrep(mystrdup(piece),"_",""); break; }
                  case 2: { phone->rules[j * 2 + 1] = mystrrep(mystrdup(piece),"_",""); break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if ((!(phone->rules[j * 2])) || (!(phone->rules[j * 2 + 1]))) {
             HUNSPELL_WARNING(stderr, "error: PHONE table is corrupt\n");
             phone->num = 0;
             return 1;
        }
   }
   phone->rules[phone->num * 2] = mystrdup("");
   phone->rules[phone->num * 2 + 1] = mystrdup("");
   init_phonet_hash(*phone);
   return 0;
}

/* parse in the checkcompoundpattern table */
int  AffixMgr::parse_checkcpdtable(char * line, FILE * af)
{
   if (numcheckcpd != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate compound pattern tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numcheckcpd = atoi(piece);
                       if (numcheckcpd < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in compound pattern table\n");
                          free(piece);
                          return 1;
                       }
                       checkcpdtable = (replentry *) malloc(numcheckcpd * sizeof(struct replentry));
                       if (!checkcpdtable) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing compound pattern table information\n");
      return 1;
   } 
 
   /* now parse the numcheckcpd lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numcheckcpd; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        checkcpdtable[j].pattern = NULL;
        checkcpdtable[j].pattern2 = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"CHECKCOMPOUNDPATTERN",20) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: compound pattern table is corrupt\n");
                                 numcheckcpd = 0;
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: { checkcpdtable[j].pattern = mystrdup(piece); break; }
                  case 2: { checkcpdtable[j].pattern2 = mystrdup(piece); break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if ((!(checkcpdtable[j].pattern)) || (!(checkcpdtable[j].pattern2))) {
             HUNSPELL_WARNING(stderr, "error: compound pattern table is corrupt\n");
	     numcheckcpd = 0;
	     return 1;
        }
   }
   return 0;
}

/* parse in the compound rule table */
int  AffixMgr::parse_defcpdtable(char * line, FILE * af)
{
   if (numdefcpd != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate compound rule tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numdefcpd = atoi(piece);
                       if (numdefcpd < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in compound rule table\n");
                          free(piece);
                          return 1;
                       }
                       defcpdtable = (flagentry *) malloc(numdefcpd * sizeof(flagentry));
                       if (!defcpdtable) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing compound rule table information\n");
      return 1;
   } 
 
   /* now parse the numdefcpd lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numdefcpd; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        defcpdtable[j].def = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece, "COMPOUNDRULE", 12) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: compound rule table is corrupt\n");
                                 free(piece);
                                 numdefcpd = 0;
                                 return 1;
                             }
                             break;
                          }
                  case 1: { 
                            defcpdtable[j].len = 
                                pHMgr->decode_flags(&(defcpdtable[j].def), piece);
                            break; 
                           }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!defcpdtable[j].len) {
             HUNSPELL_WARNING(stderr, "error: compound rule table is corrupt\n");
             numdefcpd = 0;
             return 1;
        }
   }
   return 0;
}


/* parse in the character map table */
int  AffixMgr::parse_maptable(char * line, FILE * af)
{
   if (nummap != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate MAP tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       nummap = atoi(piece);
                       if (nummap < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in map table\n");
                          free(piece);
                          return 1;
                       }
                       maptable = (mapentry *) malloc(nummap * sizeof(struct mapentry));
                       if (!maptable) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing map table information\n");
      return 1;
   } 
 
   /* now parse the nummap lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < nummap; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        maptable[j].set = NULL;
        maptable[j].len = 0;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"MAP",3) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: map table is corrupt\n");
                                 nummap = 0;
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            maptable[j].len = 0;
                            maptable[j].set = NULL;
                            maptable[j].set_utf16 = NULL;
                            if (!utf8) {
                                maptable[j].set = mystrdup(piece); 
                                maptable[j].len = strlen(maptable[j].set);
                            } else {
                                w_char w[MAXWORDLEN];
                                int n = u8_u16(w, MAXWORDLEN, piece);
                                if (n > 0) {
                                    flag_qsort((unsigned short *) w, 0, n);
                                    maptable[j].set_utf16 = (w_char *) malloc(n * sizeof(w_char));
                                    if (!maptable[j].set_utf16) return 1;
                                    memcpy(maptable[j].set_utf16, w, n * sizeof(w_char));
                                }
                                maptable[j].len = n;
                            }
                            break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if ((!(maptable[j].set || maptable[j].set_utf16)) || (!(maptable[j].len))) {
             HUNSPELL_WARNING(stderr, "error: map table is corrupt\n");
             nummap = 0;
             return 1;
        }
   }
   return 0;
}

/* parse in the word breakpoint table */
int  AffixMgr::parse_breaktable(char * line, FILE * af)
{
   if (numbreak != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate word breakpoint tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numbreak = atoi(piece);
                       if (numbreak < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in BREAK table\n");
                          free(piece);
                          return 1;
                       }
                       breaktable = (char **) malloc(numbreak * sizeof(char *));
                       if (!breaktable) return 1;
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      HUNSPELL_WARNING(stderr, "error: missing word breakpoint table information\n");
      return 1;
   } 
 
   /* now parse the numbreak lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numbreak; j++) {
        if (!fgets(nl,MAXLNLEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"BREAK",5) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: BREAK table is corrupt\n");
                                 free(piece);
                                 numbreak = 0;
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            breaktable[j] = mystrdup(piece);
                            break;
                          }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!breaktable) {
             HUNSPELL_WARNING(stderr, "error: BREAK table is corrupt\n");
             numbreak = 0;
             return 1;
        }
   }
   return 0;
}

int  AffixMgr::parse_affix(char * line, const char at, FILE * af, char * dupflags)
{
   int numents = 0;      // number of affentry structures to parse

   unsigned short aflag = 0;      // affix char identifier

   char ff=0;
   struct affentry * ptr= NULL;
   struct affentry * nptr= NULL;

   char * tp = line;
   char * nl = line;
   char * piece;
   int i = 0;

   // checking lines with bad syntax
#ifdef DEBUG
   int basefieldnum = 0;
#endif

   // split affix header line into pieces

   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
      if (*piece != '\0') {
          switch(i) {
             // piece 1 - is type of affix
             case 0: { np++; break; }
          
             // piece 2 - is affix char
             case 1: { 
                    np++;
                    aflag = pHMgr->decode_flag(piece);
                    if (((at == 'S') && (dupflags[aflag] & dupSFX)) ||
                        ((at == 'P') && (dupflags[aflag] & dupPFX))) {
                        HUNSPELL_WARNING(stderr, "error: duplicate affix flag %s in line %s\n", piece, nl);
                        // return 1; XXX permissive mode for bad dictionaries
                    }
                    dupflags[aflag] += ((at == 'S') ? dupSFX : dupPFX);
                    break; 
                    }
             // piece 3 - is cross product indicator 
             case 2: { np++; if (*piece == 'Y') ff = aeXPRODUCT; break; }

             // piece 4 - is number of affentries
             case 3: { 
                       np++;
                       numents = atoi(piece); 
                       if (numents == 0) {
                           char * err = pHMgr->encode_flag(aflag);
                           HUNSPELL_WARNING(stderr, "error: affix %s header has incorrect entry count in line %s\n",
                                   err, nl);
                           free(err);
                           return 1;
                       }
                       ptr = (struct affentry *) malloc(numents * sizeof(struct affentry));
                       if (!ptr) return 1;
                       ptr->opts = ff;
                       if (utf8) ptr->opts += aeUTF8;
                       if (pHMgr->is_aliasf()) ptr->opts += aeALIASF;
#ifdef HUNSPELL_EXPERIMENTAL
                       if (pHMgr->is_aliasm()) ptr->opts += aeALIASM;
#endif
                       ptr->aflag = aflag;
                     }

             default: break;
          }
          i++;
      }
      free(piece);
      piece = mystrsep(&tp, 0);
   }
   // check to make sure we parsed enough pieces
   if (np != 4) {
       char * err = pHMgr->encode_flag(aflag); 
       HUNSPELL_WARNING(stderr, "error: affix %s header has insufficient data in line %s\n", err, nl);
       free(err);
       free(ptr);
       return 1;
   }
 
   // store away ptr to first affentry
   nptr = ptr;

   // now parse numents affentries for this affix
   for (int j=0; j < numents; j++) {
      if (!fgets(nl,MAXLNLEN,af)) return 1;
      mychomp(nl);
      tp = nl;
      i = 0;
      np = 0;

      // split line into pieces
      piece = mystrsep(&tp, 0);
      while (piece) {
         if (*piece != '\0') {
             switch(i) {
                // piece 1 - is type
                case 0: { 
                          np++;
                          if (nptr != ptr) nptr->opts = ptr->opts;
                          break;
                        }

                // piece 2 - is affix char
                case 1: { 
                          np++;
                          if (pHMgr->decode_flag(piece) != aflag) {
                              char * err = pHMgr->encode_flag(aflag);
                              HUNSPELL_WARNING(stderr, "error: affix %s is corrupt near line %s\n", err, nl);
                              HUNSPELL_WARNING(stderr, "error: possible incorrect count\n");
                              free(err);
                              free(piece);
                              return 1;
                          }

                          if (nptr != ptr) nptr->aflag = ptr->aflag;
                          break;
                        }

                // piece 3 - is string to strip or 0 for null 
                case 2: { 
                          np++;
                          if (complexprefixes) {
                            if (utf8) reverseword_utf(piece); else reverseword(piece);
                          }
                          nptr->strip = mystrdup(piece);
                          nptr->stripl = (unsigned char) strlen(nptr->strip);
                          if (strcmp(nptr->strip,"0") == 0) {
                              free(nptr->strip);
                              nptr->strip=mystrdup("");
                              nptr->stripl = 0;
                          }   
                          break; 
                        }

                // piece 4 - is affix string or 0 for null
                case 3: { 
                          char * dash;  
#ifdef HUNSPELL_EXPERIMENTAL
                          nptr->morphcode = NULL;
#endif
                          nptr->contclass = NULL;
                          nptr->contclasslen = 0;
                          np++;
                          dash = strchr(piece, '/');
                          if (dash) {
                            *dash = '\0';

                            if (ignorechars) {
                              if (utf8) {
                                remove_ignored_chars_utf(piece, ignorechars_utf16, ignorechars_utf16_len);
                              } else {
                                remove_ignored_chars(piece,ignorechars);
                              }
                            }
                            
                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece); else reverseword(piece);
                            }
                            nptr->appnd = mystrdup(piece);
                            
                            if (pHMgr->is_aliasf()) {
                                int index = atoi(dash + 1);
                                nptr->contclasslen = (unsigned short) pHMgr->get_aliasf(index, &(nptr->contclass));
                            } else {
                                nptr->contclasslen = (unsigned short) pHMgr->decode_flags(&(nptr->contclass), dash + 1);
                                flag_qsort(nptr->contclass, 0, nptr->contclasslen);
                            }
                            *dash = '/';

                            havecontclass = 1;
                            for (unsigned short _i = 0; _i < nptr->contclasslen; _i++) {
                              contclasses[(nptr->contclass)[_i]] = 1;
                            }
                          } else {
                            if (ignorechars) {
                              if (utf8) {
                                remove_ignored_chars_utf(piece, ignorechars_utf16, ignorechars_utf16_len);
                              } else {
                                remove_ignored_chars(piece,ignorechars);
                              }
                            }

                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece); else reverseword(piece);
                            }
                            nptr->appnd = mystrdup(piece);       
                          }
                          
                          nptr->appndl = (unsigned char) strlen(nptr->appnd);
                          if (strcmp(nptr->appnd,"0") == 0) {
                              free(nptr->appnd);
                              nptr->appnd=mystrdup("");
                              nptr->appndl = 0;
                          }   
                          break; 
                        }

                // piece 5 - is the conditions descriptions
                case 4: { 
                          np++;
                          if (complexprefixes) {
                            int neg = 0;
                            if (utf8) reverseword_utf(piece); else reverseword(piece);
                            // reverse condition
                            for (char * k = piece + strlen(piece) - 1; k >= piece; k--) {
                                switch(*k) {
                                  case '[': {
                                        if (neg) *(k+1) = '['; else *k = ']';
                                        break;
                                    }
                                  case ']': {
                                        *k = '[';
                                        if (neg) *(k+1) = '^';
                                        neg = 0;
                                        break;
                                    }
                                  case '^': {
                                       if (*(k+1) == ']') neg = 1; else *(k+1) = *k;
                                       break;
                                    }
                                  default: {
                                    if (neg) *(k+1) = *k;
                                  }
                               }
                            }
                          }
                          if (nptr->stripl && (strcmp(piece, ".") != 0) &&
                            redundant_condition(at, nptr->strip, nptr->stripl, piece, nl))
                                strcpy(piece, ".");
                          if (encodeit(nptr,piece)) return 1;
                         break;
                }
                
#ifdef HUNSPELL_EXPERIMENTAL
                case 5: {
                          np++;
                          if (pHMgr->is_aliasm()) {
                            int index = atoi(piece);
                            nptr->morphcode = pHMgr->get_aliasm(index);
                          } else {
                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece); else reverseword(piece);
                            }
                            nptr->morphcode = mystrdup(piece);
                          }
                          break; 
                }
#endif

                default: break;
             }
             i++;
         }
         free(piece);
         piece = mystrsep(&tp, 0);
      }
      // check to make sure we parsed enough pieces
      if (np < 4) {
          char * err = pHMgr->encode_flag(aflag);
          HUNSPELL_WARNING(stderr, "error: affix %s is corrupt near line %s\n", err, nl);
          free(err);
          free(ptr);
          return 1;
      }

#ifdef DEBUG
#ifdef HUNSPELL_EXPERIMENTAL
      // detect unnecessary fields, excepting comments
      if (basefieldnum) {
        int fieldnum = !(nptr->morphcode) ? 5 : ((*(nptr->morphcode)=='#') ? 5 : 6);
          if (fieldnum != basefieldnum) 
            HUNSPELL_WARNING(stderr, "warning: bad field number:\n%s\n", nl);
      } else {
        basefieldnum = !(nptr->morphcode) ? 5 : ((*(nptr->morphcode)=='#') ? 5 : 6);
      }
#endif
#endif
      nptr++;
   }
 
   // now create SfxEntry or PfxEntry objects and use links to
   // build an ordered (sorted by affix string) list
   nptr = ptr;
   for (int k = 0; k < numents; k++) {
      if (at == 'P') {
          PfxEntry * pfxptr = new PfxEntry(this,nptr);
          build_pfxtree((AffEntry *)pfxptr);
      } else {
          SfxEntry * sfxptr = new SfxEntry(this,nptr);
          build_sfxtree((AffEntry *)sfxptr); 
      }
      nptr++;
   }      
   free(ptr);
   return 0;
}

int AffixMgr::redundant_condition(char ft, char * strip, int stripl, const char * cond, char * warnvar) {
  int condl = strlen(cond);
  int i;
  int j;
  int neg;
  int in;
  if (ft == 'P') { // prefix
    if (strncmp(strip, cond, condl) == 0) return 1;
    if (utf8) {
    } else {
      for (i = 0, j = 0; (i < stripl) && (j < condl); i++, j++) {
        if (cond[j] != '[') {
          if (cond[j] != strip[i]) {
            HUNSPELL_WARNING(stderr, "warning: incompatible stripping characters and condition:\n%s\n", warnvar);
          }
        } else {
          neg = (cond[j+1] == '^') ? 1 : 0;
          in = 0;
          do {
            j++;
            if (strip[i] == cond[j]) in = 1;
          } while ((j < (condl - 1)) && (cond[j] != ']'));
          if (j == (condl - 1) && (cond[j] != ']')) {
            HUNSPELL_WARNING(stderr, "error: missing ] in condition:\n%s\n", warnvar);
            return 0;
          }
          if ((!neg && !in) || (neg && in)) {
            HUNSPELL_WARNING(stderr, "warning: incompatible stripping characters and condition:\n%s\n", warnvar);
            return 0;          
          }
        }
      }
      if (j >= condl) return 1;
    }
  } else { // suffix
    if ((stripl >= condl) && strcmp(strip + stripl - condl, cond) == 0) return 1;
    if (utf8) {
    } else {
      for (i = stripl - 1, j = condl - 1; (i >= 0) && (j >= 0); i--, j--) {
        if (cond[j] != ']') {
          if (cond[j] != strip[i]) {
            HUNSPELL_WARNING(stderr, "warning: incompatible stripping characters and condition:\n%s\n", warnvar);
          }
        } else {
          in = 0;
          do {
            j--;
            if (strip[i] == cond[j]) in = 1;
          } while ((j > 0) && (cond[j] != '['));
          if ((j == 0) && (cond[j] != '[')) {
            HUNSPELL_WARNING(stderr, "error: missing ] in condition:\n%s\n", warnvar);
            return 0;
          }
          neg = (cond[j+1] == '^') ? 1 : 0;
          if ((!neg && !in) || (neg && in)) {
            HUNSPELL_WARNING(stderr, "warning: incompatible stripping characters and condition:\n%s\n", warnvar);
            return 0;          
          }
        }
      }
      if (j < 0) return 1;
    }    
  }
  return 0;
}
