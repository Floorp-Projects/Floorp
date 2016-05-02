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
#include <limits>

#include "replist.hxx"
#include "csutil.hxx"

RepList::RepList(int n) {
  dat = (replentry**)malloc(sizeof(replentry*) * n);
  if (dat == 0)
    size = 0;
  else
    size = n;
  pos = 0;
}

RepList::~RepList() {
  for (int i = 0; i < pos; i++) {
    free(dat[i]->pattern);
    free(dat[i]->pattern2);
    free(dat[i]);
  }
  free(dat);
}

int RepList::get_pos() {
  return pos;
}

replentry* RepList::item(int n) {
  return dat[n];
}

int RepList::near(const char* word) {
  int p1 = 0;
  int p2 = pos;
  while ((p2 - p1) > 1) {
    int m = (p1 + p2) / 2;
    int c = strcmp(word, dat[m]->pattern);
    if (c <= 0) {
      if (c < 0)
        p2 = m;
      else
        p1 = p2 = m;
    } else
      p1 = m;
  }
  return p1;
}

int RepList::match(const char* word, int n) {
  if (strncmp(word, dat[n]->pattern, strlen(dat[n]->pattern)) == 0)
    return strlen(dat[n]->pattern);
  return 0;
}

int RepList::add(char* pat1, char* pat2) {
  if (pos >= size || pat1 == NULL || pat2 == NULL)
    return 1;
  replentry* r = (replentry*)malloc(sizeof(replentry));
  if (r == NULL)
    return 1;
  r->pattern = mystrrep(pat1, "_", " ");
  r->pattern2 = mystrrep(pat2, "_", " ");
  r->start = false;
  r->end = false;
  dat[pos++] = r;
  for (int i = pos - 1; i > 0; i--) {
    r = dat[i];
    if (strcmp(r->pattern, dat[i - 1]->pattern) < 0) {
      dat[i] = dat[i - 1];
      dat[i - 1] = r;
    } else
      break;
  }
  return 0;
}

int RepList::conv(const char* word, char* dest, size_t destsize) {
  size_t stl = 0;
  int change = 0;
  for (size_t i = 0; i < strlen(word); i++) {
    int n = near(word + i);
    int l = match(word + i, n);
    if (l) {
      size_t replen = strlen(dat[n]->pattern2);
      if (stl + replen >= destsize)
        return -1;
      strcpy(dest + stl, dat[n]->pattern2);
      stl += replen;
      i += l - 1;
      change = 1;
    } else {
      if (stl + 1 >= destsize)
        return -1;
      dest[stl++] = word[i];
    }
  }
  dest[stl] = '\0';
  return change;
}

bool RepList::conv(const char* word, std::string& dest) {
  dest.clear();

  bool change = false;
  for (size_t i = 0; i < strlen(word); i++) {
    int n = near(word + i);
    int l = match(word + i, n);
    if (l) {
      dest.append(dat[n]->pattern2);
      i += l - 1;
      change = true;
    } else {
      dest.push_back(word[i]);
    }
  }
  return change;
}
