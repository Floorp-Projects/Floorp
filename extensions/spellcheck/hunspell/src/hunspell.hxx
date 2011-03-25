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
 *                 Caolan McNamara (caolanm@redhat.com)
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

#include "hunvisapi.h"

#include "hashmgr.hxx"
#include "affixmgr.hxx"
#include "suggestmgr.hxx"
#include "langnum.hxx"

#define  SPELL_XML "<?xml?>"

#define MAXDIC 20
#define MAXSUGGESTION 15
#define MAXSHARPS 5

#define HUNSPELL_OK       (1 << 0)
#define HUNSPELL_OK_WARN  (1 << 1)

#ifndef _MYSPELLMGR_HXX_
#define _MYSPELLMGR_HXX_

class LIBHUNSPELL_DLL_EXPORTED Hunspell
{
  AffixMgr*       pAMgr;
  HashMgr*        pHMgr[MAXDIC];
  int             maxdic;
  SuggestMgr*     pSMgr;
  char *          affixpath;
  char *          encoding;
  struct cs_info * csconv;
  int             langnum;
  int             utf8;
  int             complexprefixes;
  char**          wordbreak;

public:

  /* Hunspell(aff, dic) - constructor of Hunspell class
   * input: path of affix file and dictionary file
   */

  Hunspell(const char * affpath, const char * dpath, const char * key = NULL);
  ~Hunspell();

  /* load extra dictionaries (only dic files) */
  int add_dic(const char * dpath, const char * key = NULL);

  /* spell(word) - spellcheck word
   * output: 0 = bad word, not 0 = good word
   *   
   * plus output:
   *   info: information bit array, fields:
   *     SPELL_COMPOUND  = a compound word 
   *     SPELL_FORBIDDEN = an explicit forbidden word
   *   root: root (stem), when input is a word with affix(es)
   */
   
  int spell(const char * word, int * info = NULL, char ** root = NULL);

  /* suggest(suggestions, word) - search suggestions
   * input: pointer to an array of strings pointer and the (bad) word
   *   array of strings pointer (here *slst) may not be initialized
   * output: number of suggestions in string array, and suggestions in
   *   a newly allocated array of strings (*slts will be NULL when number
   *   of suggestion equals 0.)
   */

  int suggest(char*** slst, const char * word);

  /* deallocate suggestion lists */

  void free_list(char *** slst, int n);

  char * get_dic_encoding();

 /* morphological functions */

 /* analyze(result, word) - morphological analysis of the word */
 
  int analyze(char*** slst, const char * word);

 /* stem(result, word) - stemmer function */
  
  int stem(char*** slst, const char * word);
  
 /* stem(result, analysis, n) - get stems from a morph. analysis
  * example:
  * char ** result, result2;
  * int n1 = analyze(&result, "words");
  * int n2 = stem(&result2, result, n1);   
  */
 
  int stem(char*** slst, char ** morph, int n);

 /* generate(result, word, word2) - morphological generation by example(s) */

  int generate(char*** slst, const char * word, const char * word2);

 /* generate(result, word, desc, n) - generation by morph. description(s)
  * example:
  * char ** result;
  * char * affix = "is:plural"; // description depends from dictionaries, too
  * int n = generate(&result, "word", &affix, 1);
  * for (int i = 0; i < n; i++) printf("%s\n", result[i]);
  */

  int generate(char*** slst, const char * word, char ** desc, int n);

  /* functions for run-time modification of the dictionary */

  /* add word to the run-time dictionary */
  
  int add(const char * word);

  /* add word to the run-time dictionary with affix flags of
   * the example (a dictionary word): Hunspell will recognize
   * affixed forms of the new word, too.
   */
  
  int add_with_affix(const char * word, const char * example);

  /* remove word from the run-time dictionary */

  int remove(const char * word);

  /* other */

  /* get extra word characters definied in affix file for tokenization */
  const char * get_wordchars();
  unsigned short * get_wordchars_utf16(int * len);

  struct cs_info * get_csconv();
  const char * get_version();

  int get_langnum() const;
  
  /* experimental and deprecated functions */

#ifdef HUNSPELL_EXPERIMENTAL
  /* suffix is an affix flag string, similarly in dictionary files */  
  int put_word_suffix(const char * word, const char * suffix);
  char * morph_with_correction(const char * word);

  /* spec. suggestions */
  int suggest_auto(char*** slst, const char * word);
  int suggest_pos_stems(char*** slst, const char * word);
#endif

private:
   int    cleanword(char *, const char *, int * pcaptype, int * pabbrev);
   int    cleanword2(char *, const char *, w_char *, int * w_len, int * pcaptype, int * pabbrev);
   void   mkinitcap(char *);
   int    mkinitcap2(char * p, w_char * u, int nc);
   int    mkinitsmall2(char * p, w_char * u, int nc);
   void   mkallcap(char *);
   int    mkallcap2(char * p, w_char * u, int nc);
   void   mkallsmall(char *);
   int    mkallsmall2(char * p, w_char * u, int nc);
   struct hentry * checkword(const char *, int * info, char **root);
   char * sharps_u8_l1(char * dest, char * source);
   hentry * spellsharps(char * base, char *, int, int, char * tmp, int * info, char **root);
   int    is_keepcase(const hentry * rv);
   int    insert_sug(char ***slst, char * word, int ns);
   void   cat_result(char * result, char * st);
   char * stem_description(const char * desc);
   int    spellml(char*** slst, const char * word);
   int    get_xml_par(char * dest, const char * par, int maxl);
   const char * get_xml_pos(const char * s, const char * attr);
   int    get_xml_list(char ***slst, char * list, const char * tag);
   int    check_xml_par(const char * q, const char * attr, const char * value);

};

#endif
