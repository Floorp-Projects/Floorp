/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsICharsetAlias_h___
#define nsICharsetAlias_h___

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

// {CCD4D374-CCDC-11d2-B3B1-00805F8A6670}
#define NS_ICHARSETALIAS_IID \
{ 0xccd4d374, 0xccdc, 0x11d2, { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}
static NS_DEFINE_IID(kICharsetAliasIID, NS_ICHARSETALIAS_IID);


// {98D41C21-CCF3-11d2-B3B1-00805F8A6670}
#define NS_CHARSETALIAS_CID \
{ 0x98d41c21, 0xccf3, 0x11d2, { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}
static NS_DEFINE_IID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

#define NS_CHARSETALIAS_CID \
{ 0x98d41c21, 0xccf3, 0x11d2, { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}

#define NS_CHARSETALIAS_CONTRACTID "@mozilla.org/intl/charsetalias;1"

class nsICharsetAlias : public nsISupports
{
public:
   NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICHARSETALIAS_IID)

   NS_IMETHOD GetPreferred(const nsAString& aAlias, nsAString& aResult) = 0;
   NS_IMETHOD GetPreferred(const PRUnichar* aAlias, const PRUnichar** aResult) = 0;
   NS_IMETHOD GetPreferred(const char* aAlias, char* aResult, PRInt32 aBufLength) = 0;

   NS_IMETHOD Equals(const nsAString& aCharset1, const nsAString& aCharset2, PRBool* aResult) = 0;
   NS_IMETHOD Equals(const PRUnichar* aCharset1, const PRUnichar* aCharset2, PRBool* aResult) = 0;
   NS_IMETHOD Equals(const char* aCharset1, const char* aCharset2, PRBool* aResult) = 0;

};

#endif /* nsICharsetAlias_h___ */
