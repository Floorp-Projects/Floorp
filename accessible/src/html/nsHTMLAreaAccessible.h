/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author: Aaron Leventhal (aaronl@netscape.com)
 * Contributor(s): 
 */

#ifndef _nsHTMLAreaAccessible_H_
#define _nsHTMLAreaAccessible_H_

#include "nsGenericAccessible.h"
#include "nsAccessible.h"

/* Accessible for image map areas - must be child of image
 */

class nsHTMLAreaAccessible : public nsLinkableAccessible
{

public:
  nsHTMLAreaAccessible(nsIDOMNode *domNode, nsIAccessible *accParent, nsIWeakReference* aShell);
  NS_IMETHOD GetAccName(nsAWritableString & _retval); 
  NS_IMETHOD GetAccRole(PRUint32 *_retval); 
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccParent(nsIAccessible * *aAccParent);
  NS_IMETHOD GetAccNextSibling(nsIAccessible * *aAccNextSibling);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible * *aAccPreviousSibling);
  NS_IMETHOD GetAccDescription(nsAWritableString& _retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);

protected:
  nsIAccessible *CreateAreaAccessible(nsIDOMNode *aDOMNode);
  nsCOMPtr<nsIAccessible> mAccParent;
};

#endif  
