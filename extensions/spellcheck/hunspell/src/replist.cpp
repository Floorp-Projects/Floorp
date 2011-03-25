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
 * Contributor(s): László Németh (nemethl@gyorsposta.hu)
 *                 Caolan McNamara (caolanm@redhat.com)
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "replist.hxx"
#include "csutil.hxx"

RepList::RepList(int n) {
    dat = (replentry **) malloc(sizeof(replentry *) * n);
    if (dat == 0) size = 0; else size = n;
    pos = 0;
}

RepList::~RepList()
{
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

replentry * RepList::item(int n) {
    return dat[n];
}

int RepList::near(const char * word) {
    int p1 = 0;
    int p2 = pos;
    while ((p2 - p1) > 1) {
      int m = (p1 + p2) / 2;
      int c = strcmp(word, dat[m]->pattern);
      if (c <= 0) {
        if (c < 0) p2 = m; else p1 = p2 = m;
      } else p1 = m;
    }
    return p1;
}

int RepList::match(const char * word, int n) {
    if (strncmp(word, dat[n]->pattern, strlen(dat[n]->pattern)) == 0) return strlen(dat[n]->pattern);
    return 0;
}

int RepList::add(char * pat1, char * pat2) {
    if (pos >= size || pat1 == NULL || pat2 == NULL) return 1;
    replentry * r = (replentry *) malloc(sizeof(replentry));
    if (r == NULL) return 1;
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
      } else break;
    }
    return 0;
}

int RepList::conv(const char * word, char * dest) {
    int stl = 0;
    int change = 0;
    for (size_t i = 0; i < strlen(word); i++) {
        int n = near(word + i);
        int l = match(word + i, n);
        if (l) {
          strcpy(dest + stl, dat[n]->pattern2);
          stl += strlen(dat[n]->pattern2);
          i += l - 1;
          change = 1;
        } else dest[stl++] = word[i];
    }
    dest[stl] = '\0';
    return change;
}
