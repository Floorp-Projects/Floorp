/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsICharsetAlias_h___
#define nsICharsetAlias_h___

#include "nscore.h"
#include "nsISupports.h"

// {CCD4D374-CCDC-11d2-B3B1-00805F8A6670}
NS_DECLARE_ID(kICharsetAliasIID,
{ 0xccd4d374, 0xccdc, 0x11d2, \
    { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };


class nsICharsetAlias : public nsISupports
{
public:
   NS_IMETHOD GetPreferred(nsString& aAlias, nsString& aResult) = 0;
   NS_IMETHOD GetPreferred(const PRUnichar* aAlias, const PRUnichar** aResult) = 0;
   NS_IMETHOD GetPreferred(const char* aAlias, const char** aResult) = 0;

   NS_IMETHOD Equals(nsString& aCharset, nsString& aCharset, PRBool* aResult) = 0;
   NS_IMETHOD Equals(const PRUnichar* aCharset, const PRUnichar* aCharset, PRBool* aResult) = 0;
   NS_IMETHOD Equals(const char* aCharset, const char* aCharset, PRBool* aResult) = 0;

};

#endif /* nsICharsetAlias_h___ */
