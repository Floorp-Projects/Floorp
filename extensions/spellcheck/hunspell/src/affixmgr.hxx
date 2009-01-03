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

#ifndef _AFFIXMGR_HXX_
#define _AFFIXMGR_HXX_

#ifdef MOZILLA_CLIENT
#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif
#include <stdio.h>
#else
#include <cstdio>
#endif

#include "atypes.hxx"
#include "baseaffix.hxx"
#include "hashmgr.hxx"
#include "phonet.hxx"
#include "replist.hxx"

// check flag duplication
#define dupSFX        (1 << 0)
#define dupPFX        (1 << 1)

class AffixMgr
{

  AffEntry *          pStart[SETSIZE];
  AffEntry *          sStart[SETSIZE];
  AffEntry *          pFlag[SETSIZE];
  AffEntry *          sFlag[SETSIZE];
  HashMgr *           pHMgr;
  HashMgr **          alldic;
  int *               maxdic;
  char *              keystring;
  char *              trystring;
  char *              encoding;
  struct cs_info *    csconv;
  int                 utf8;
  int                 complexprefixes;
  FLAG                compoundflag;
  FLAG                compoundbegin;
  FLAG                compoundmiddle;
  FLAG                compoundend;
  FLAG                compoundroot;
  FLAG                compoundforbidflag;
  FLAG                compoundpermitflag;
  int                 checkcompounddup;
  int                 checkcompoundrep;
  int                 checkcompoundcase;
  int                 checkcompoundtriple;
  int                 simplifiedtriple;
  FLAG                forbiddenword;
  FLAG                nosuggest;
  FLAG                needaffix;
  int                 cpdmin;
  int                 numrep;
  replentry *         reptable;
  RepList *           iconvtable;
  RepList *           oconvtable;
  int                 nummap;
  mapentry *          maptable;
  int                 numbreak;
  char **             breaktable;
  int                 numcheckcpd;
  patentry *          checkcpdtable;
  int                 simplifiedcpd;
  int                 numdefcpd;
  flagentry *         defcpdtable;
  phonetable *        phone;
  int                 maxngramsugs;
  int                 nosplitsugs;
  int                 sugswithdots;
  int                 cpdwordmax;
  int                 cpdmaxsyllable;
  char *              cpdvowels;
  w_char *            cpdvowels_utf16;
  int                 cpdvowels_utf16_len;
  char *              cpdsyllablenum;
  const char *        pfxappnd; // BUG: not stateless
  const char *        sfxappnd; // BUG: not stateless
  FLAG                sfxflag;  // BUG: not stateless
  char *              derived;  // BUG: not stateless
  AffEntry *          sfx;      // BUG: not stateless
  AffEntry *          pfx;      // BUG: not stateless
  int                 checknum;
  char *              wordchars;
  unsigned short *    wordchars_utf16;
  int                 wordchars_utf16_len;
  char *              ignorechars;
  unsigned short *    ignorechars_utf16;
  int                 ignorechars_utf16_len;
  char *              version;
  char *              lang;
  int                 langnum;
  FLAG                lemma_present;
  FLAG                circumfix;
  FLAG                onlyincompound;
  FLAG                keepcase;
  FLAG                substandard;
  int                 checksharps;
  int                 fullstrip;

  int                 havecontclass; // boolean variable
  char                contclasses[CONTSIZE]; // flags of possible continuing classes (twofold affix)

public:

  AffixMgr(const char * affpath, HashMgr** ptr, int * md,
    const char * key = NULL);
  ~AffixMgr();
  struct hentry *     affix_check(const char * word, int len,
            const unsigned short needflag = (unsigned short) 0,
            char in_compound = IN_CPD_NOT);
  struct hentry *     prefix_check(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  inline int isSubset(const char * s1, const char * s2);
  struct hentry *     prefix_check_twosfx(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  inline int isRevSubset(const char * s1, const char * end_of_s2, int len);
  struct hentry *     suffix_check(const char * word, int len, int sfxopts,
            AffEntry* ppfx, char ** wlst, int maxSug, int * ns,
            const FLAG cclass = FLAG_NULL, const FLAG needflag = FLAG_NULL,
            char in_compound = IN_CPD_NOT);
  struct hentry *     suffix_check_twosfx(const char * word, int len,
            int sfxopts, AffEntry* ppfx, const FLAG needflag = FLAG_NULL);

  char * affix_check_morph(const char * word, int len,
            const FLAG needflag = FLAG_NULL, char in_compound = IN_CPD_NOT);
  char * prefix_check_morph(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  char * suffix_check_morph (const char * word, int len, int sfxopts,
            AffEntry * ppfx, const FLAG cclass = FLAG_NULL,
            const FLAG needflag = FLAG_NULL, char in_compound = IN_CPD_NOT);

  char * prefix_check_twosfx_morph(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  char * suffix_check_twosfx_morph(const char * word, int len,
            int sfxopts, AffEntry * ppfx, const FLAG needflag = FLAG_NULL);

  char * morphgen(char * ts, int wl, const unsigned short * ap,
            unsigned short al, char * morph, char * targetmorph, int level);

  int    expand_rootword(struct guessword * wlst, int maxn, const char * ts,
            int wl, const unsigned short * ap, unsigned short al, char * bad,
            int, char *);

  short       get_syllable (const char * word, int wlen);
  int         cpdrep_check(const char * word, int len);
  int         cpdpat_check(const char * word, int len, hentry * r1, hentry * r2);
  int         defcpd_check(hentry *** words, short wnum, hentry * rv,
                    hentry ** rwords, char all);
  int         cpdcase_check(const char * word, int len);
  inline int  candidate_check(const char * word, int len);
  void        setcminmax(int * cmin, int * cmax, const char * word, int len);
  struct hentry * compound_check(const char * word, int len, short wordnum,
            short numsyllable, short maxwordnum, short wnum, hentry ** words,
            char hu_mov_rule, char is_sug);

  int compound_check_morph(const char * word, int len, short wordnum,
            short numsyllable, short maxwordnum, short wnum, hentry ** words,
            char hu_mov_rule, char ** result, char * partresult);

  struct hentry * lookup(const char * word);
  int                 get_numrep();
  struct replentry *  get_reptable();
  RepList *           get_iconvtable();
  RepList *           get_oconvtable();
  struct phonetable * get_phonetable();
  int                 get_nummap();
  struct mapentry *   get_maptable();
  int                 get_numbreak();
  char **             get_breaktable();
  char *              get_encoding();
  int                 get_langnum();
  char *              get_key_string();
  char *              get_try_string();
  const char *        get_wordchars();
  unsigned short *    get_wordchars_utf16(int * len);
  char *              get_ignore();
  unsigned short *    get_ignore_utf16(int * len);
  int                 get_compound();
  FLAG                get_compoundflag();
  FLAG                get_compoundbegin();
  FLAG                get_forbiddenword();
  FLAG                get_nosuggest();
  FLAG                get_needaffix();
  FLAG                get_onlyincompound();
  FLAG                get_compoundroot();
  FLAG                get_lemma_present();
  int                 get_checknum();
  char *              get_possible_root();
  const char *        get_prefix();
  const char *        get_suffix();
  const char *        get_derived();
  const char *        get_version();
  const int           have_contclass();
  int                 get_utf8();
  int                 get_complexprefixes();
  char *              get_suffixed(char );
  int                 get_maxngramsugs();
  int                 get_nosplitsugs();
  int                 get_sugswithdots(void);
  FLAG                get_keepcase(void);
  int                 get_checksharps(void);
  char *              encode_flag(unsigned short aflag);
  int                 get_fullstrip();

private:
  int  parse_file(const char * affpath, const char * key);
  int  parse_flag(char * line, unsigned short * out, FileMgr * af);
  int  parse_num(char * line, int * out, FileMgr * af);
  int  parse_cpdsyllable(char * line, FileMgr * af);
  int  parse_reptable(char * line, FileMgr * af);
  int  parse_convtable(char * line, FileMgr * af, RepList ** rl, const char * keyword);
  int  parse_phonetable(char * line, FileMgr * af);
  int  parse_maptable(char * line, FileMgr * af);
  int  parse_breaktable(char * line, FileMgr * af);
  int  parse_checkcpdtable(char * line, FileMgr * af);
  int  parse_defcpdtable(char * line, FileMgr * af);
  int  parse_affix(char * line, const char at, FileMgr * af, char * dupflags);

  void reverse_condition(char *);
  void debugflag(char * result, unsigned short flag);
  int condlen(char *);
  int encodeit(struct affentry * ptr, char * cs);
  int build_pfxtree(AffEntry* pfxptr);
  int build_sfxtree(AffEntry* sfxptr);
  int process_pfx_order();
  int process_sfx_order();
  AffEntry * process_pfx_in_order(AffEntry * ptr, AffEntry * nptr);
  AffEntry * process_sfx_in_order(AffEntry * ptr, AffEntry * nptr);
  int process_pfx_tree_to_list();
  int process_sfx_tree_to_list();
  int redundant_condition(char, char * strip, int stripl,
      const char * cond, int);
};

#endif
