/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 */

#ifndef NSDEFAULTURIFIXUP_H
#define NSDEFAULTURIFIXUP_H

#include "nsIPref.h"
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
    NS_IMETHOD FileURIFixup(const PRUnichar* aStringURI, nsIURI** aURI);
    NS_IMETHOD ConvertFileToStringURI(nsString& aIn, nsString& aOut);
    NS_IMETHOD ConvertStringURIToFileCharset(nsString& aIn, nsCString& aOut);
    NS_IMETHOD KeywordURIFixup(const PRUnichar* aStringURI, nsIURI** aURI);

    nsCOMPtr<nsIPref> mPrefs;
};

#endif
