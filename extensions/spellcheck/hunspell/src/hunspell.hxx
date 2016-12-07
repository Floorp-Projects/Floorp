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

#include "hunvisapi.h"

#include "hashmgr.hxx"
#include "affixmgr.hxx"
#include "suggestmgr.hxx"
#include "langnum.hxx"
#include <vector>

#define SPELL_XML "<?xml?>"

#define MAXDIC 20
#define MAXSUGGESTION 15
#define MAXSHARPS 5

#define HUNSPELL_OK (1 << 0)
#define HUNSPELL_OK_WARN (1 << 1)

#ifndef _MYSPELLMGR_HXX_
#define _MYSPELLMGR_HXX_

class LIBHUNSPELL_DLL_EXPORTED Hunspell {
 private:
  Hunspell(const Hunspell&);
  Hunspell& operator=(const Hunspell&);

 private:
  AffixMgr* pAMgr;
  HashMgr* pHMgr[MAXDIC];
  int maxdic;
  SuggestMgr* pSMgr;
  char* affixpath;
  char* encoding;
  struct cs_info* csconv;
  int langnum;
  int utf8;
  int complexprefixes;
  char** wordbreak;

 public:
  /* Hunspell(aff, dic) - constructor of Hunspell class
   * input: path of affix file and dictionary file
   *
   * In WIN32 environment, use UTF-8 encoded paths started with the long path
   * prefix \\\\?\\ to handle system-independent character encoding and very
   * long path names (without the long path prefix Hunspell will use fopen()
   * with system-dependent character encoding instead of _wfopen()).
   */

  Hunspell(const char* affpath, const char* dpath, const char* key = NULL);
  ~Hunspell();

  /* load extra dictionaries (only dic files) */
  int add_dic(const char* dpath, const char* key = NULL);

  /* spell(word) - spellcheck word
   * output: 0 = bad word, not 0 = good word
   *
   * plus output:
   *   info: information bit array, fields:
   *     SPELL_COMPOUND  = a compound word
   *     SPELL_FORBIDDEN = an explicit forbidden word
   *   root: root (stem), when input is a word with affix(es)
   */

  int spell(const char* word, int* info = NULL, char** root = NULL);

  /* suggest(suggestions, word) - search suggestions
   * input: pointer to an array of strings pointer and the (bad) word
   *   array of strings pointer (here *slst) may not be initialized
   * output: number of suggestions in string array, and suggestions in
   *   a newly allocated array of strings (*slts will be NULL when number
   *   of suggestion equals 0.)
   */

  int suggest(char*** slst, const char* word);

  /* Suggest words from suffix rules
   * suffix_suggest(suggestions, root_word)
   * input: pointer to an array of strings pointer and the  word
   *   array of strings pointer (here *slst) may not be initialized
   * output: number of suggestions in string array, and suggestions in
   *   a newly allocated array of strings (*slts will be NULL when number
   *   of suggestion equals 0.)
   */
  int suffix_suggest(char*** slst, const char* root_word);

  /* deallocate suggestion lists */

  void free_list(char*** slst, int n);

  char* get_dic_encoding();

  /* morphological functions */

  /* analyze(result, word) - morphological analysis of the word */

  int analyze(char*** slst, const char* word);

  /* stem(result, word) - stemmer function */

  int stem(char*** slst, const char* word);

  /* stem(result, analysis, n) - get stems from a morph. analysis
   * example:
   * char ** result, result2;
   * int n1 = analyze(&result, "words");
   * int n2 = stem(&result2, result, n1);
   */

  int stem(char*** slst, char** morph, int n);

  /* generate(result, word, word2) - morphological generation by example(s) */

  int generate(char*** slst, const char* word, const char* word2);

  /* generate(result, word, desc, n) - generation by morph. description(s)
   * example:
   * char ** result;
   * char * affix = "is:plural"; // description depends from dictionaries, too
   * int n = generate(&result, "word", &affix, 1);
   * for (int i = 0; i < n; i++) printf("%s\n", result[i]);
   */

  int generate(char*** slst, const char* word, char** desc, int n);

  /* functions for run-time modification of the dictionary */

  /* add word to the run-time dictionary */

  int add(const char* word);

  /* add word to the run-time dictionary with affix flags of
   * the example (a dictionary word): Hunspell will recognize
   * affixed forms of the new word, too.
   */

  int add_with_affix(const char* word, const char* example);

  /* remove word from the run-time dictionary */

  int remove(const char* word);

  /* other */

  /* get extra word characters definied in affix file for tokenization */
  const char* get_wordchars();
  const std::vector<w_char>& get_wordchars_utf16();

  struct cs_info* get_csconv();
  const char* get_version();

  int get_langnum() const;

  /* need for putdic */
  int input_conv(const char* word, char* dest, size_t destsize);

 private:
  void cleanword(std::string& dest, const char*, int* pcaptype, int* pabbrev);
  size_t cleanword2(std::string& dest,
                    std::vector<w_char>& dest_u,
                    const char*,
                    int* w_len,
                    int* pcaptype,
                    size_t* pabbrev);
  void mkinitcap(std::string& u8);
  int mkinitcap2(std::string& u8, std::vector<w_char>& u16);
  int mkinitsmall2(std::string& u8, std::vector<w_char>& u16);
  void mkallcap(std::string& u8);
  int mkallsmall2(std::string& u8, std::vector<w_char>& u16);
  struct hentry* checkword(const char*, int* info, char** root);
  std::string sharps_u8_l1(const std::string& source);
  hentry*
  spellsharps(std::string& base, size_t start_pos, int, int, int* info, char** root);
  int is_keepcase(const hentry* rv);
  int insert_sug(char*** slst, const char* word, int ns);
  void cat_result(std::string& result, char* st);
  char* stem_description(const char* desc);
  int spellml(char*** slst, const char* word);
  std::string get_xml_par(const char* par);
  const char* get_xml_pos(const char* s, const char* attr);
  int get_xml_list(char*** slst, const char* list, const char* tag);
  int check_xml_par(const char* q, const char* attr, const char* value);
};

#endif
