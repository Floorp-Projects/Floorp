/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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
 * Simple replacement for hnjalloc.h from libhyphen-2.x, to use moz_x* memory
 * allocation functions. Note that the hyphen.c code does *NOT* check for
 * NULL from memory (re)allocation, so it is essential that we use the
 * "infallible" moz_x* variants here.
 */

#include "mozilla/mozalloc.h"

#define hnj_malloc(size)      moz_xmalloc(size)
#define hnj_realloc(p, size)  moz_xrealloc(p, size)
#define hnj_free(p)           moz_free(p)

/*
 * To enable us to load hyphenation dictionaries from arbitrary resource URIs,
 * not just through file paths using stdio, we override the (few) stdio APIs
 * that hyphen.c uses and provide our own reimplementation that calls Gecko
 * i/o methods.
 */

#include <stdio.h> /* ensure stdio.h is loaded before our macros */

#undef FILE
#define FILE hnjFile

#define fopen(path,mode)      hnjFopen(path,mode)
#define fclose(file)          hnjFclose(file)
#define fgets(buf,count,file) hnjFgets(buf,count,file)

typedef struct hnjFile_ hnjFile;

#ifdef __cplusplus
extern "C" {
#endif

hnjFile* hnjFopen(const char* aURISpec, const char* aMode);

int hnjFclose(hnjFile* f);

char* hnjFgets(char* s, int n, hnjFile* f);

#ifdef __cplusplus
}
#endif


