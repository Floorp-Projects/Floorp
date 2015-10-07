/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is meant to be used with |#define CSS_REPORT_PARSE_ERRORS|
 * in mozilla/dom/html/style/src/nsCSSScanner.h uncommented, and the
 * |#ifdef DEBUG| block in nsCSSScanner::OutputError (in
 * nsCSSScanner.cpp in the same directory) used (even if not a debug
 * build).
 */

#include "nsXPCOM.h"
#include "nsCOMPtr.h"

#include "nsIFile.h"
#include "nsNetUtil.h"

#include "nsContentCID.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/css/Loader.h"

using namespace mozilla;

static already_AddRefed<nsIURI>
FileToURI(const char *aFilename, nsresult *aRv = 0)
{
    nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, aRv));
    NS_ENSURE_TRUE(lf, nullptr);
    // XXX Handle relative paths somehow.
    lf->InitWithNativePath(nsDependentCString(aFilename));

    nsIURI *uri = nullptr;
    nsresult rv = NS_NewFileURI(&uri, lf);
    if (aRv)
        *aRv = rv;
    return uri;
}

static int
ParseCSSFile(nsIURI *aSheetURI)
{
    RefPtr<mozilla::css::Loader> = new mozilla::css::Loader();
    RefPtr<CSSStyleSheet> sheet;
    loader->LoadSheetSync(aSheetURI, getter_AddRefs(sheet));
    NS_ASSERTION(sheet, "sheet load failed");
    /* This can happen if the file can't be found (e.g. you
     * ask for a relative path and xpcom/io rejects it)
     */
    if (!sheet)
        return -1;
    bool complete;
    sheet->GetComplete(complete);
    NS_ASSERTION(complete, "synchronous load did not complete");
    if (!complete)
        return -2;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "%s [FILE]...\n", argv[0]);
    }
    nsresult rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv))
        return (int)rv;

    int res = 0;
    for (int i = 1; i < argc; ++i) {
        const char *filename = argv[i];

        printf("\nParsing %s.\n", filename);

        nsCOMPtr<nsIURI> uri = FileToURI(filename, &rv);
        if (rv == NS_ERROR_OUT_OF_MEMORY) {
            fprintf(stderr, "Out of memory.\n");
            return 1;
        }
        if (uri)
            res = ParseCSSFile(uri);
    }

    NS_ShutdownXPCOM(nullptr);

    return res;
}
