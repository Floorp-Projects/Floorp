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

#ifndef _nsIFrameRootAccessible_H_
#define _nsIFrameRootAccessible_H_

#include "nsBaseWidgetAccessible.h"
#include "nsRootAccessible.h"
#include "nsAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleEventReceiver.h"

class nsIWebShell;
class nsIWeakReference;

class nsHTMLIFrameAccessible : public nsBlockAccessible, 
                               public nsIAccessibleDocument,
                               public nsIAccessibleHyperText,
                               public nsIAccessibleEventReceiver,
                               public nsDocAccessibleMixin
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEDOCUMENT
  NS_DECL_NSIACCESSIBLEHYPERTEXT

  public:
    nsHTMLIFrameAccessible(nsIDOMNode* aNode, nsIAccessible* aRoot, nsIWeakReference* aShell, nsIDocument *doc);

    NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
    NS_IMETHOD GetAccName(nsAString& aAccName);
    NS_IMETHOD GetAccValue(nsAString& AccValue);
    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
    NS_IMETHOD GetAccState(PRUint32 *aAccState);

    // ----- nsIAccessibleEventReceiver -------------------
    NS_IMETHOD AddAccessibleEventListener(nsIAccessibleEventListener *aListener);
    NS_IMETHOD RemoveAccessibleEventListener();

  protected:
    nsCOMPtr<nsIAccessible> mRootAccessible;

    //helper function for nsIAccessibleHyperText
    PRBool IsHyperLink(nsIAccessible *aAccNode);
    PRInt32 GetLinksFromAccNode(nsIAccessible *aAccNode);
    nsresult GetLinkFromAccNode(PRInt32 aIndex, nsIAccessible *aAccNode,
                                nsIAccessibleHyperLink **_retval);
    PRInt32 GetAccNodeCharLength(nsIAccessible *aAccNode);
    nsresult GetLinkIndexFromAccNode(nsIAccessible *aAccNode,
                                     PRInt32 aCharIndex, PRInt32 *_retval);

};

class nsHTMLIFrameRootAccessible : public nsRootAccessible
{
  NS_DECL_ISUPPORTS_INHERITED

  public:
    nsHTMLIFrameRootAccessible(nsIWeakReference* aShell);
    virtual ~nsHTMLIFrameRootAccessible();

    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);

    // ----- nsIAccessibleEventReceiver -------------------
    NS_IMETHOD AddAccessibleEventListener(nsIAccessibleEventListener *aListener);
    NS_IMETHOD RemoveAccessibleEventListener();
};


#endif  
