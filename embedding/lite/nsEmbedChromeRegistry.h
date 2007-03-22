/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIChromeRegistry.h"
#include "nsSupportsArray.h"

// {0C3434A5-D52E-4bb7-BEBE-F572D8EDBE2B}
#define NS_EMBEDCHROMEREGISTRY_CID \
  { 0xc3434a5, 0xd52e, 0x4bb7, \
    { 0xbe, 0xbe, 0xf5, 0x72, 0xd8, 0xed, 0xbe, 0x2b } }


class nsEmbedChromeRegistry : public nsIChromeRegistry
{
public:
    nsEmbedChromeRegistry();
    virtual ~nsEmbedChromeRegistry() {};
    nsresult Init();
    
    NS_DECL_ISUPPORTS

    NS_DECL_NSICHROMEREGISTRY

    nsresult ReadChromeRegistry();
    nsresult ProcessNewChromeBuffer(char* aBuffer, PRInt32 aLength);
    PRInt32 ProcessChromeLine(const char* aBuffer, PRInt32 aLength);
    nsresult RegisterChrome(const nsACString& aChromeType,
                            const nsACString& aChromeProfile,
                            const nsACString& aChromeLocType,
                            const nsACString& aChromeLocation);
    nsresult RegisterChrome(PRInt32 aChromeType, // CHROME_TYPE_CONTENT, etc
                            PRBool aChromeIsProfile, // per-profile?
                            PRBool aChromeIsURL, // is it a url? (else path)
                            const nsACString& aChromeLocation);


    
private:
    nsCOMPtr<nsISupportsArray> mEmptyArray;
};
