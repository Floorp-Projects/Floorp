/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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


