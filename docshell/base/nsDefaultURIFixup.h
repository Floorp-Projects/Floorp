/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDEFAULTURIFIXUP_H
#define NSDEFAULTURIFIXUP_H

#include "nsIURIFixup.h"

class nsDefaultURIFixupInfo;

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
    nsresult FixupURIProtocol(const nsACString& aIn, nsIURI** aURI);
    void KeywordURIFixup(const nsACString &aStringURI,
                         nsDefaultURIFixupInfo* aFixupInfo,
                         nsIInputStream** aPostData);
    bool PossiblyByteExpandedFileName(const nsAString& aIn);
    bool PossiblyHostPortUrl(const nsACString& aUrl);
    bool MakeAlternateURI(nsIURI *aURI);
    bool IsLikelyFTP(const nsCString& aHostSpec);
};

class nsDefaultURIFixupInfo : public nsIURIFixupInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURIFIXUPINFO

    nsDefaultURIFixupInfo(const nsACString& aOriginalInput);

    friend class nsDefaultURIFixup;

protected:
    virtual ~nsDefaultURIFixupInfo();

private:
    nsCOMPtr<nsISupports> mConsumer;
    nsCOMPtr<nsIURI> mPreferredURI;
    nsCOMPtr<nsIURI> mFixedURI;
    bool mFixupUsedKeyword;
    bool mInputHasProtocol;
    bool mInputHostHasDot;
    nsAutoCString mOriginalInput;
};
#endif
