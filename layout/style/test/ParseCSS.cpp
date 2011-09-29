/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is L. David Baron.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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
 * This file is meant to be used with |#define CSS_REPORT_PARSE_ERRORS|
 * in mozilla/content/html/style/src/nsCSSScanner.h uncommented, and the
 * |#ifdef DEBUG| block in nsCSSScanner::OutputError (in
 * nsCSSScanner.cpp in the same directory) used (even if not a debug
 * build).
 */

#include "nsXPCOM.h"
#include "nsCOMPtr.h"

#include "nsILocalFile.h"
#include "nsNetUtil.h"

#include "nsContentCID.h"
#include "mozilla/css/Loader.h"
#include "nsCSSStyleSheet.h"

static already_AddRefed<nsIURI>
FileToURI(const char *aFilename, nsresult *aRv = 0)
{
    nsCOMPtr<nsILocalFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, aRv));
    NS_ENSURE_TRUE(lf, nsnull);
    // XXX Handle relative paths somehow.
    lf->InitWithNativePath(nsDependentCString(aFilename));

    nsIURI *uri = nsnull;
    nsresult rv = NS_NewFileURI(&uri, lf);
    if (aRv)
        *aRv = rv;
    return uri;
}

static int
ParseCSSFile(nsIURI *aSheetURI)
{
    nsRefPtr<mozilla::css::Loader> = new mozilla::css::Loader();
    nsRefPtr<nsCSSStyleSheet> sheet;
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
    nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
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

    NS_ShutdownXPCOM(nsnull);

    return res;
}
