/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDEFAULTURIFIXUP_H
#define NSDEFAULTURIFIXUP_H

#include "nsIURIFixup.h"

#include "nsCOMPtr.h"

#include "nsCDefaultURIFixup.h"

/* Header file */
class nsDefaultURIFixup : public nsIURIFixup
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURIFIXUP

    nsDefaultURIFixup();

protected:
    virtual ~nsDefaultURIFixup();

private:
    /* additional members */
    nsresult FileURIFixup(const nsACString &aStringURI, nsIURI** aURI);
    nsresult ConvertFileToStringURI(const nsACString& aIn, nsCString& aOut);
    void KeywordURIFixup(const nsACString &aStringURI, nsIInputStream** aPostData, nsIURI** aURI);
    bool PossiblyByteExpandedFileName(const nsAString& aIn);
    bool PossiblyHostPortUrl(const nsACString& aUrl);
    bool MakeAlternateURI(nsIURI *aURI);
    bool IsLikelyFTP(const nsCString& aHostSpec);
    const char * GetFileSystemCharset();
    const char * GetCharsetForUrlBar();

    nsCString mFsCharset;
};

#endif
