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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#ifndef nsIContentList_h___
#define nsIContentList_h___

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

// d72cb400-4b33-11d5-a041-0010a4ef48c9
#define NS_ICONTENTLIST_IID \
{ 0xd72cb400, 0x4b33, 0x11d5, { 0xa0, 0x41, 0x0, 0x10, 0xa4, 0xef, 0x48, 0xc9 } }

/**
 * Interface for content list.
 */
class nsIContentList : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENTLIST_IID)

  // Callers will want to pass in PR_TRUE for aDoFlush unless they
  // are explicitly avoiding an FlushPendingNotifications.  The
  // flush guarantees that the list will be up to date.

  NS_IMETHOD GetLength(PRUint32* aLength, PRBool aDoFlush) = 0;

  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn,
                  PRBool aDoFlush) = 0;

  NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn,
                       PRBool aDoFlush) = 0;

  NS_IMETHOD IndexOf(nsIContent *aContent, PRInt32& aIndex,
                     PRBool aDoFlush) = 0;
};

#endif /* nsIContentList_h___ */
