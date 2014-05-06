/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file provides substitutes for the basic stdio routines used by hyphen.c
// to read its dictionary files. We #define the stdio names to these versions
// in hnjalloc.h, so that we can use nsIURI and nsIInputStream to specify and
// access the dictionary resources.

#include "hnjalloc.h"
#undef FILE // Undo the damage done in hnjalloc.h
#include "nsNetUtil.h"

#define BUFSIZE 1024

struct hnjFile_ {
    nsCOMPtr<nsIInputStream> mStream;
    char                     mBuffer[BUFSIZE];
    uint32_t                 mCurPos;
    uint32_t                 mLimit;
};

// replacement for fopen()
// (not a full substitute: only supports read access)
hnjFile*
hnjFopen(const char* aURISpec, const char* aMode)
{
    // this override only needs to support "r"
    NS_ASSERTION(!strcmp(aMode, "r"), "unsupported fopen() mode in hnjFopen");

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURISpec);
    if (NS_FAILED(rv)) {
        return nullptr;
    }

    nsCOMPtr<nsIInputStream> instream;
    rv = NS_OpenURI(getter_AddRefs(instream), uri);
    if (NS_FAILED(rv)) {
        return nullptr;
    }

    hnjFile *f = new hnjFile;
    f->mStream = instream;
    f->mCurPos = 0;
    f->mLimit = 0;

    return f;
}

// replacement for fclose()
int
hnjFclose(hnjFile* f)
{
    NS_ASSERTION(f && f->mStream, "bad argument to hnjFclose");

    int result = 0;
    nsresult rv = f->mStream->Close();
    if (NS_FAILED(rv)) {
        result = EOF;
    }
    f->mStream = nullptr;

    delete f;
    return result;
}

// replacement for fgets()
// (not a full reimplementation, but sufficient for libhyphen's needs)
char*
hnjFgets(char* s, int n, hnjFile* f)
{
    NS_ASSERTION(s && f, "bad argument to hnjFgets");

    int i = 0;
    while (i < n - 1) {
        if (f->mCurPos < f->mLimit) {
            char c = f->mBuffer[f->mCurPos++];
            s[i++] = c;
            if (c == '\n' || c == '\r') {
                break;
            }
            continue;
        }

        f->mCurPos = 0;

        nsresult rv = f->mStream->Read(f->mBuffer, BUFSIZE, &f->mLimit);
        if (NS_FAILED(rv)) {
            f->mLimit = 0;
            return nullptr;
        }

        if (f->mLimit == 0) {
            break;
        }
    }

    if (i == 0) {
        return nullptr; // end of file
    }

    s[i] = '\0'; // null-terminate the returned string
    return s;
}
