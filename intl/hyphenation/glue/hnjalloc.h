/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * To enable us to load hyphenation dictionaries from arbitrary resource URIs,
 * not just through file paths using stdio, we override the (few) stdio APIs
 * that hyphen.c uses and provide our own reimplementation that calls Gecko
 * i/o methods.
 */

#include <stdio.h> /* ensure stdio.h is loaded before our macros */

#undef FILE
#define FILE hnjFile

#define fopen(path, mode) hnjFopen(path, mode)
#define fclose(file) hnjFclose(file)
#define fgets(buf, count, file) hnjFgets(buf, count, file)
#define feof(file) hnjFeof(file)
#define fgetc(file) hnjFgetc(file)

typedef struct hnjFile_ hnjFile;

#ifdef __cplusplus
extern "C" {
#endif

void* hnj_malloc(size_t size);
void* hnj_realloc(void* ptr, size_t size);
void hnj_free(void* ptr);

hnjFile* hnjFopen(const char* aURISpec, const char* aMode);

int hnjFclose(hnjFile* f);

char* hnjFgets(char* s, int n, hnjFile* f);

int hnjFeof(hnjFile* f);

int hnjFgetc(hnjFile* f);

#ifdef __cplusplus
}
#endif
