/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by: Rajiv Dayal <rdayal@netscape.com> 
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

#ifndef nsAbIPCCard_h__
#define nsAbIPCCard_h__

#include "nsAbMDBCardProperty.h"
#include "nsIAbMDBCard.h"
#include "nsISupportsArray.h"
#include "IPalmSync.h"

// these are states of Palm record
// as defined in the Palm CDK
#define ATTR_DELETED        0x0001
#define ATTR_ARCHIVED       0x0002
#define ATTR_MODIFIED       0x0004
#define ATTR_NEW            0x0008
#define ATTR_NONE           0x0020
#define ATTR_NO_REC         0x0040

class nsAbIPCCard : public nsAbMDBCardProperty
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    
    // this checks for all card data fields
    NS_IMETHOD Equals(nsIAbCard *card, PRBool *_retval);
    PRBool Equals(nsABCOMCardStruct * card, nsStringArray & differingAttrs);
    PRBool EqualsAfterUnicodeConversion(nsABCOMCardStruct * card, nsStringArray & differingAttrs);
    
    // this check names attribs: first name + last name + display name + nickname
    PRBool Same(nsABCOMCardStruct * card, PRBool isUnicode=PR_TRUE);
    PRBool Same(nsIAbCard *card);

    NS_IMETHOD Copy(nsIAbCard *srcCard);
    nsresult Copy(nsABCOMCardStruct * srcCard);
    nsresult ConvertToUnicodeAndCopy(nsABCOMCardStruct * srcCard);

    // this function will allocate new memory for the passed in card struct data members
    // the caller needs to CoTaskMemFree once it is done using the card struct
    nsresult GetABCOMCardStruct(PRBool isUnicode, nsABCOMCardStruct * card);

    nsAbIPCCard();
    nsAbIPCCard(nsIAbCard *card);
    nsAbIPCCard(nsABCOMCardStruct *card, PRBool isUnicode=PR_TRUE);
    virtual ~nsAbIPCCard();

    void SetStatus(PRUint32 status) { mStatus = status; }
    PRUint32 GetStatus() { return mStatus; }
    void SetRecordId(PRUint32 recID) { mRecordId = recID; }
    PRUint32 GetRecordId() { return mRecordId; }
    void SetCategoryId(PRUint32 catID) { mCategoryId = catID; }
    PRUint32 GetCategoryId() { return mCategoryId; }

private:
    PRUint32 mRecordId;
    PRUint32 mCategoryId;
    PRUint32 mStatus;

    void CopyValue(PRBool isUnicode, nsString & attribValue, LPTSTR * result);
    PRBool CompareValue(PRBool isUnicode, LPTSTR cardValue, nsString & attribValue);
};

#endif
