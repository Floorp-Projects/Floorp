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
 * and Laszlo Nemeth (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Kevin Hendricks (kevin.hendricks@sympatico.ca)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 David Einstein (deinst@world.std.com)
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

#include "hashmgr.hxx"
#include "csutil.hxx"
#include "atypes.hxx"

#ifdef MOZILLA_CLIENT
#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif
#else
#ifndef W32
using namespace std;
#endif
#endif

// build a hash table from a munched word list

HashMgr::HashMgr(const char * tpath, const char * apath)
{
  tablesize = 0;
  tableptr = NULL;
  flag_mode = FLAG_CHAR;
  complexprefixes = 0;
  utf8 = 0;
  langnum = 0;
  lang = NULL;
  enc = NULL;
  csconv = 0;
  ignorechars = NULL;
  ignorechars_utf16 = NULL;
  ignorechars_utf16_len = 0;
  numaliasf = 0;
  aliasf = NULL;
  numaliasm = 0;
  aliasm = NULL;
  forbiddenword = FLAG_NULL; // forbidden word signing flag
  load_config(apath);  
  int ec = load_tables(tpath);
  if (ec) {
    /* error condition - what should we do here */
    HUNSPELL_WARNING(stderr, "Hash Manager Error : %d\n",ec);
    if (tableptr) {
      free(tableptr);
    }
    tablesize = 0;
  }
}


HashMgr::~HashMgr()
{
  if (tableptr) {
    // now pass through hash table freeing up everything
    // go through column by column of the table
    for (int i=0; i < tablesize; i++) {
      struct hentry * pt = &tableptr[i];
      struct hentry * nt = NULL;
      if (pt) {
        if (pt->astr && !aliasf) free(pt->astr);
        if (pt->word) free(pt->word);
#ifdef HUNSPELL_EXPERIMENTAL
        if (pt->description && !aliasm) free(pt->description);
#endif
        pt = pt->next;
      }
      while(pt) {
        nt = pt->next;
        if (pt->astr && !aliasf) free(pt->astr);
        if (pt->word) free(pt->word);
#ifdef HUNSPELL_EXPERIMENTAL
        if (pt->description && !aliasm) free(pt->description);
#endif
        free(pt);
        pt = nt;
      }
    }
    free(tableptr);
  }
  tablesize = 0;

  if (aliasf) {
    for (int j = 0; j < (numaliasf); j++) free(aliasf[j]);
    free(aliasf);
    aliasf = NULL;
    if (aliasflen) {
      free(aliasflen);
      aliasflen = NULL;
    }
  }
  if (aliasm) {
    for (int j = 0; j < (numaliasm); j++) free(aliasm[j]);
    free(aliasm);
    aliasm = NULL;
  }  

  if (enc) free(enc);
  if (lang) free(lang);
  
  if (ignorechars) free(ignorechars);
  if (ignorechars_utf16) free(ignorechars_utf16);
}

// lookup a root word in the hashtable

struct hentry * HashMgr::lookup(const char *word) const
{
    struct hentry * dp;
    if (tableptr) {
       dp = &tableptr[hash(word)];
       if (dp->word == NULL) return NULL;
       for (  ;  dp != NULL;  dp = dp->next) {
          if (strcmp(word,dp->word) == 0) return dp;
       }
    }
    return NULL;
}

// add a word to the hash table (private)

int HashMgr::add_word(const char * word, int wl, unsigned short * aff,
    int al, const char * desc, bool onlyupcase)
{
    char * st = mystrdup(word);
    bool upcasehomonym = false;
    if (wl && !st) return 1;
    if (ignorechars != NULL) {
      if (utf8) {
        remove_ignored_chars_utf(st, ignorechars_utf16, ignorechars_utf16_len);
      } else {
        remove_ignored_chars(st, ignorechars);
      }
    }
    if (complexprefixes) {
        if (utf8) reverseword_utf(st); else reverseword(st);
    }
    int i = hash(st);
    struct hentry * dp = &tableptr[i];
    if (dp->word == NULL) {
       dp->wlen = (short) wl;
       dp->alen = (short) al;
       dp->word = st;
       dp->astr = aff;
       dp->next = NULL;
       dp->next_homonym = NULL;
#ifdef HUNSPELL_EXPERIMENTAL
       if (aliasm) {
            dp->description = (desc) ? get_aliasm(atoi(desc)) : mystrdup(desc);
       } else {
            dp->description = mystrdup(desc);
            if (desc && !dp->description) return 1;
            if (dp->description && complexprefixes) {
                if (utf8) reverseword_utf(dp->description); else reverseword(dp->description);
            }
       }
#endif
    } else {
       struct hentry* hp = (struct hentry *) malloc (sizeof(struct hentry));
       if (!hp) return 1;
       hp->wlen = (short) wl;
       hp->alen = (short) al;
       hp->word = st;
       hp->astr = aff;
       hp->next = NULL;      
       hp->next_homonym = NULL;
#ifdef HUNSPELL_EXPERIMENTAL       
       if (aliasm) {
            hp->description = (desc) ? get_aliasm(atoi(desc)) : mystrdup(desc);
       } else {
            hp->description = mystrdup(desc);
            if (desc && !hp->description) return 1;
            if (dp->description && complexprefixes) {
                if (utf8) reverseword_utf(hp->description); else reverseword(hp->description);
            }
       }
#endif
       while (dp->next != NULL) {
         if ((!dp->next_homonym) && (strcmp(hp->word, dp->word) == 0)) {
    	    // remove hidden onlyupcase homonym
            if (!onlyupcase) {
		if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
		    free(dp->astr);
		    dp->astr = hp->astr;
		    free(hp->word);
		    free(hp);
		    return 0;
		} else {
    		    dp->next_homonym = hp;
    		}
            } else {
        	upcasehomonym = true;
            }
         }
         dp=dp->next;
       }
       if (strcmp(hp->word, dp->word) == 0) {
    	    // remove hidden onlyupcase homonym
            if (!onlyupcase) {
		if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
		    free(dp->astr);
		    dp->astr = hp->astr;
		    free(hp->word);
		    free(hp);
		    return 0;
		} else {
    		    dp->next_homonym = hp;
    		}
            } else {
        	upcasehomonym = true;
            }
       }
       if (!upcasehomonym) {
    	    dp->next = hp;
       } else {
    	    // remove hidden onlyupcase homonym
    	    free(hp->word);
    	    if (hp->astr) free(hp->astr);
    	    free(hp);
       }
    }
    return 0;
}     

// add a custom dic. word to the hash table (public)
int HashMgr::put_word(const char * word, int wl, char * aff)
{
    unsigned short * flags;
    int al = 0;
    if (aff) {
        al = decode_flags(&flags, aff);
        flag_qsort(flags, 0, al);
    } else {
        flags = NULL;
    }
    add_word(word, wl, flags, al, NULL, false);
    return 0;
}

int HashMgr::put_word_pattern(const char * word, int wl, const char * pattern)
{
    unsigned short * flags;
    struct hentry * dp = lookup(pattern);
    if (!dp || !dp->astr) return 1;
    flags = (unsigned short *) malloc (dp->alen * sizeof(short));
    memcpy((void *) flags, (void *) dp->astr, dp->alen * sizeof(short));
    add_word(word, wl, flags, dp->alen, NULL, false);
    return 0;
}

// walk the hash table entry by entry - null at end
struct hentry * HashMgr::walk_hashtable(int &col, struct hentry * hp) const
{
  //reset to start
  if ((col < 0) || (hp == NULL)) {
    col = -1;
    hp = NULL;
  }

  if (hp && hp->next != NULL) {
    hp = hp->next;
  } else {
    col++;
    hp = (col < tablesize) ? &tableptr[col] : NULL;
    // search for next non-blank column entry
    while (hp && (hp->word == NULL)) {
        col ++;
        hp = (col < tablesize) ? &tableptr[col] : NULL;
    }
    if (col < tablesize) return hp;
    hp = NULL;
    col = -1;
  }
  return hp;
}

// load a munched word list and build a hash table on the fly
int HashMgr::load_tables(const char * tpath)
{
  int wl, al;
  char * ap;
  char * dp;
  unsigned short * flags;
  int captype;

  // raw dictionary - munched file
  FILE * rawdict = fopen(tpath, "r");
  if (rawdict == NULL) return 1;

  // first read the first line of file to get hash table size */
  char ts[MAXDELEN];
  if (! fgets(ts, MAXDELEN-1,rawdict)) {
    HUNSPELL_WARNING(stderr, "error: empty dic file\n");
    fclose(rawdict);
    return 2;
  }
  mychomp(ts);
  
  /* remove byte order mark */
  if (strncmp(ts,"\xEF\xBB\xBF",3) == 0) {
    memmove(ts, ts+3, strlen(ts+3)+1);
    HUNSPELL_WARNING(stderr, "warning: dic file begins with byte order mark: possible incompatibility with old Hunspell versions\n");
  }
  
  if ((*ts < '1') || (*ts > '9')) HUNSPELL_WARNING(stderr, "error - missing word count in dictionary file\n");
  tablesize = atoi(ts);
  if (!tablesize) {
    fclose(rawdict);
    return 4;
  }
  tablesize = tablesize + 5 + USERWORD;
  if ((tablesize %2) == 0) tablesize++;

  // allocate the hash table
  tableptr = (struct hentry *) calloc(tablesize, sizeof(struct hentry));
  if (! tableptr) {
    fclose(rawdict);
    return 3;
  }
  for (int i=0; i<tablesize; i++) tableptr[i].word = NULL;

  // loop through all words on much list and add to hash
  // table and create word and affix strings

  while (fgets(ts,MAXDELEN-1,rawdict)) {
    mychomp(ts);
    // split each line into word and morphological description
    dp = strchr(ts,'\t');

    if (dp) {
      *dp = '\0';
      dp++;
    } else {
      dp = NULL;
    }

    // split each line into word and affix char strings
    // "\/" signs slash in words (not affix separator)
    // "/" at beginning of the line is word character (not affix separator)
    ap = strchr(ts,'/');
    while (ap) {
        if (ap == ts) {
            ap++;
            continue;
        } else if (*(ap - 1) != '\\') break;
        // replace "\/" with "/"
        for (char * sp = ap - 1; *sp; *sp = *(sp + 1), sp++);
        ap = strchr(ap,'/');
    }

    if (ap) {
      *ap = '\0';
      if (aliasf) {
        int index = atoi(ap + 1);
        al = get_aliasf(index, &flags);
        if (!al) {
            HUNSPELL_WARNING(stderr, "error - bad flag vector alias: %s\n", ts);
            *ap = '\0';
        }
      } else {
        al = decode_flags(&flags, ap + 1);
        flag_qsort(flags, 0, al);
      }
    } else {
      al = 0;
      ap = NULL;
      flags = NULL;
    }

    wl = strlen(ts);

    // add the word and its index
    if (add_word(ts,wl,flags,al,dp, false)) {
	fclose(rawdict);
	return 5;
    }

    // add decapizatalized forms to handle following cases
    // OpenOffice.org -> OPENOFFICE.ORG
    // CIA's -> CIA'S
    captype = utf8 ? get_captype_utf8(ts, wl, langnum) : get_captype(ts, wl, csconv);
    if (((captype == HUHCAP) || (captype == HUHINITCAP) ||
      ((captype == ALLCAP) && (flags != NULL))) &&
      !((flags != NULL) && TESTAFF(flags, forbiddenword, al))) {
          unsigned short * flags2 = (unsigned short *) malloc (sizeof(unsigned short *)* (al + 1));
          memcpy(flags2, flags, al * sizeof(unsigned short *));
          flags2[al] = ONLYUPCASEFLAG;
          if (utf8) {
              char st[MAXDELEN];
              w_char w[MAXDELEN];
              int wlen = u8_u16(w, MAXDELEN, ts);
              mkallsmall_utf(w, wlen, langnum);
              mkallcap_utf(w, 1, langnum);
              u16_u8(st, MAXDELEN, w, wlen);
              if (add_word(st,wl,flags2,al+1,dp, true)) {
	        fclose(rawdict);
                return 5;
              }
           } else {
               mkallsmall(ts, csconv);
               mkinitcap(ts, csconv);
               if (add_word(ts,wl,flags2,al+1,dp, true)) {
	         fclose(rawdict);
        	 return 5;
               }
           }
    }
  }
 
  fclose(rawdict);
  return 0;
}


// the hash function is a simple load and rotate
// algorithm borrowed

int HashMgr::hash(const char * word) const
{
    long  hv = 0;
    for (int i=0; i < 4  &&  *word != 0; i++)
        hv = (hv << 8) | (*word++);
    while (*word != 0) {
      ROTATE(hv,ROTATE_LEN);
      hv ^= (*word++);
    }
    return (unsigned long) hv % tablesize;
}

int HashMgr::decode_flags(unsigned short ** result, char * flags) {
    int len;
    switch (flag_mode) {
      case FLAG_LONG: { // two-character flags (1x2yZz -> 1x 2y Zz)
        len = strlen(flags);
        if (len%2 == 1) HUNSPELL_WARNING(stderr, "error: length of FLAG_LONG flagvector is odd: %s\n", flags);
        len = len/2;
        *result = (unsigned short *) malloc(len * sizeof(short));
        for (int i = 0; i < len; i++) {
            (*result)[i] = (((unsigned short) flags[i * 2]) << 8) + (unsigned short) flags[i * 2 + 1]; 
        }
        break;
      }
      case FLAG_NUM: { // decimal numbers separated by comma (4521,23,233 -> 4521 23 233)
        len = 1;
        char * src = flags; 
        unsigned short * dest;
        char * p;
        for (p = flags; *p; p++) {
          if (*p == ',') len++;
        }
        *result = (unsigned short *) malloc(len * sizeof(short));
        dest = *result;
        for (p = flags; *p; p++) {
          if (*p == ',') {
            *dest = (unsigned short) atoi(src);
            if (*dest == 0) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
            src = p + 1;
            dest++;
          }
        }
        *dest = (unsigned short) atoi(src);
        if (*dest == 0) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
        break;
      }    
      case FLAG_UNI: { // UTF-8 characters
        w_char w[MAXDELEN/2];
        len = u8_u16(w, MAXDELEN/2, flags);
        *result = (unsigned short *) malloc(len * sizeof(short));
        memcpy(*result, w, len * sizeof(short));
        break;
      }
      default: { // Ispell's one-character flags (erfg -> e r f g)
        unsigned short * dest;
        len = strlen(flags);
        *result = (unsigned short *) malloc(len * sizeof(short));
        dest = *result;
        for (unsigned char * p = (unsigned char *) flags; *p; p++) {
          *dest = (unsigned short) *p;
          dest++;
        }
      }
    }      
    return len;
}

unsigned short HashMgr::decode_flag(const char * f) {
    unsigned short s = 0;
    switch (flag_mode) {
      case FLAG_LONG:
        s = ((unsigned short) f[0] << 8) + (unsigned short) f[1];
        break;
      case FLAG_NUM:
        s = (unsigned short) atoi(f);
        break;
      case FLAG_UNI:
        u8_u16((w_char *) &s, 1, f);
        break;
      default:
        s = (unsigned short) *((unsigned char *)f);
    }
    if (!s) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
    return s;
}

char * HashMgr::encode_flag(unsigned short f) {
    unsigned char ch[10];
    if (f==0) return mystrdup("(NULL)");
    if (flag_mode == FLAG_LONG) {
        ch[0] = (unsigned char) (f >> 8);
        ch[1] = (unsigned char) (f - ((f >> 8) << 8));
        ch[2] = '\0';
    } else if (flag_mode == FLAG_NUM) {
        sprintf((char *) ch, "%d", f);
    } else if (flag_mode == FLAG_UNI) {
        u16_u8((char *) &ch, 10, (w_char *) &f, 1);
    } else {
        ch[0] = (unsigned char) (f);
        ch[1] = '\0';
    }
    return mystrdup((char *) ch);
}

// read in aff file and set flag mode
int  HashMgr::load_config(const char * affpath)
{
  int firstline = 1;
  
  // io buffers
  char line[MAXDELEN+1];
 
  // open the affix file
  FILE * afflst;
  afflst = fopen(affpath,"r");
  if (!afflst) {
    HUNSPELL_WARNING(stderr, "Error - could not open affix description file %s\n",affpath);
    return 1;
  }

    // read in each line ignoring any that do not
    // start with a known line type indicator

    while (fgets(line,MAXDELEN,afflst)) {
        mychomp(line);

       /* remove byte order mark */
       if (firstline) {
         firstline = 0;
         if (strncmp(line,"\xEF\xBB\xBF",3) == 0) memmove(line, line+3, strlen(line+3)+1);
       }

        /* parse in the try string */
        if ((strncmp(line,"FLAG",4) == 0) && isspace(line[4])) {
            if (flag_mode != FLAG_CHAR) {
                HUNSPELL_WARNING(stderr, "error: duplicate FLAG parameter\n");
            }
            if (strstr(line, "long")) flag_mode = FLAG_LONG;
            if (strstr(line, "num")) flag_mode = FLAG_NUM;
            if (strstr(line, "UTF-8")) flag_mode = FLAG_UNI;
            if (flag_mode == FLAG_CHAR) {
                HUNSPELL_WARNING(stderr, "error: FLAG need `num', `long' or `UTF-8' parameter: %s\n", line);
            }
        }
        if (strncmp(line,"FORBIDDENWORD",13) == 0) {
          char * st = NULL;
          if (parse_string(line, &st, "FORBIDDENWORD")) {
             fclose(afflst);
             return 1;
          }
          forbiddenword = decode_flag(st);
          free(st);
        }
        if (strncmp(line, "SET", 3) == 0) {
    	  if (parse_string(line, &enc, "SET")) {
             fclose(afflst);
             return 1;
          }    	    
    	  if (strcmp(enc, "UTF-8") == 0) {
    	    utf8 = 1;
#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
    	    initialize_utf_tbl();
#endif
#endif
    	  } else csconv = get_current_cs(enc);
    	}
        if (strncmp(line, "LANG", 4) == 0) {    	
    	  if (parse_string(line, &lang, "LANG")) {
             fclose(afflst);
             return 1;
          }    	    
    	  langnum = get_lang_num(lang);
    	}

       /* parse in the ignored characters (for example, Arabic optional diacritics characters */
       if (strncmp(line,"IGNORE",6) == 0) {
          if (parse_array(line, &ignorechars, &ignorechars_utf16, &ignorechars_utf16_len, "IGNORE", utf8)) {
             fclose(afflst);
             return 1;
          }
       }

       if ((strncmp(line,"AF",2) == 0) && isspace(line[2])) {
          if (parse_aliasf(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

#ifdef HUNSPELL_EXPERIMENTAL
       if ((strncmp(line,"AM",2) == 0) && isspace(line[2])) {
          if (parse_aliasm(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }
#endif
        if (strncmp(line,"COMPLEXPREFIXES",15) == 0) complexprefixes = 1;
        if (((strncmp(line,"SFX",3) == 0) || (strncmp(line,"PFX",3) == 0)) && isspace(line[3])) break;
    }
    if (csconv == NULL) csconv = get_current_cs("ISO8859-1");
    fclose(afflst);
    return 0;
}

/* parse in the ALIAS table */
int  HashMgr::parse_aliasf(char * line, FILE * af)
{
   if (numaliasf != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate AF (alias for flag vector) tables used\n");
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
                       numaliasf = atoi(piece);
                       if (numaliasf < 1) {
                          numaliasf = 0;
                          aliasf = NULL;
                          aliasflen = NULL;
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in AF table\n");
                          free(piece);
                          return 1;
                       }
                       aliasf = (unsigned short **) malloc(numaliasf * sizeof(unsigned short *));
                       aliasflen = (unsigned short *) malloc(numaliasf * sizeof(short));
                       if (!aliasf || !aliasflen) {
                          numaliasf = 0;
                          if (aliasf) free(aliasf);
                          if (aliasflen) free(aliasflen);
                          aliasf = NULL;
                          aliasflen = NULL;
                          return 1;
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
   if (np != 2) {
      numaliasf = 0;
      free(aliasf);
      free(aliasflen);
      aliasf = NULL;
      aliasflen = NULL;
      HUNSPELL_WARNING(stderr, "error: missing AF table information\n");
      return 1;
   } 
 
   /* now parse the numaliasf lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numaliasf; j++) {
        if (!fgets(nl,MAXDELEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasf[j] = NULL;
        aliasflen[j] = 0;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"AF",2) != 0) {
                                 numaliasf = 0;
                                 free(aliasf);
                                 free(aliasflen);
                                 aliasf = NULL;
                                 aliasflen = NULL;
                                 HUNSPELL_WARNING(stderr, "error: AF table is corrupt\n");
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            aliasflen[j] = (unsigned short) decode_flags(&(aliasf[j]), piece);
                            flag_qsort(aliasf[j], 0, aliasflen[j]);
                            break; 
                          }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!aliasf[j]) {
             free(aliasf);
             free(aliasflen);
             aliasf = NULL;
             aliasflen = NULL;
             numaliasf = 0;
             HUNSPELL_WARNING(stderr, "error: AF table is corrupt\n");
             return 1;
        }
   }
   return 0;
}

int HashMgr::is_aliasf() {
    return (aliasf != NULL);
}

int HashMgr::get_aliasf(int index, unsigned short ** fvec) {
    if ((index > 0) && (index <= numaliasf)) {
        *fvec = aliasf[index - 1];
        return aliasflen[index - 1];
    }
    HUNSPELL_WARNING(stderr, "error: bad flag alias index: %d\n", index);
    *fvec = NULL;
    return 0;
}

#ifdef HUNSPELL_EXPERIMENTAL
/* parse morph alias definitions */
int  HashMgr::parse_aliasm(char * line, FILE * af)
{
   if (numaliasm != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate AM (aliases for morphological descriptions) tables used\n");
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
                       numaliasm = atoi(piece);
                       if (numaliasm < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in AM table\n");
                          free(piece);
                          return 1;
                       }
                       aliasm = (char **) malloc(numaliasm * sizeof(char *));
                       if (!aliasm) {
                          numaliasm = 0;
                          return 1;
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
   if (np != 2) {
      numaliasm = 0;
      free(aliasm);
      aliasm = NULL;
      HUNSPELL_WARNING(stderr, "error: missing AM alias information\n");
      return 1;
   } 

   /* now parse the numaliasm lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numaliasm; j++) {
        if (!fgets(nl,MAXDELEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasm[j] = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"AM",2) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: AM table is corrupt\n");
                                 free(piece);
                                 numaliasm = 0;
                                 free(aliasm);
                                 aliasm = NULL;
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece);
                                    else reverseword(piece);
                            }
                            aliasm[j] = mystrdup(piece);
                            break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!aliasm[j]) {
             numaliasm = 0;
             free(aliasm);
             aliasm = NULL;
             HUNSPELL_WARNING(stderr, "error: map table is corrupt\n");
             return 1;
        }
   }
   return 0;
}

int HashMgr::is_aliasm() {
    return (aliasm != NULL);
}

char * HashMgr::get_aliasm(int index) {
    if ((index > 0) && (index <= numaliasm)) return aliasm[index - 1];
    HUNSPELL_WARNING(stderr, "error: bad morph. alias index: %d\n", index);
    return NULL;
}
#endif
