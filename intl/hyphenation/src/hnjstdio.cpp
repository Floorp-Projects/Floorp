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
 * The Original Code is Mozilla Hyphenation Service.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

// This file provides substitutes for the basic stdio routines used by hyphen.c
// to read its dictionary files. We #define the stdio names to these versions
// in hnjalloc.h, so that we can use nsIURI and nsIInputStream to specify and
// access the dictionary resources.

#include "hnjalloc.h"
#include "nsNetUtil.h"

#define BUFSIZE 1024

struct hnjFile_ {
    nsCOMPtr<nsIInputStream> mStream;
    char                     mBuffer[BUFSIZE];
    PRUint32                 mCurPos;
    PRUint32                 mLimit;
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
