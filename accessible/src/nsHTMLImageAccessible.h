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

#ifndef _nsHTMLImageAccessible_H_
#define _nsHTMLImageAccessible_H_

#include "nsAccessible.h"
#include "nsIFrame.h"
#include "nsIImageFrame.h"
#include "nsIDOMHTMLMapElement.h"

/* Accessible for supporting images
 * supports:
 * - gets name, role
 * - support basic state
 */
class nsHTMLImageAccessible : public nsLinkableAccessible
{

public:
  nsHTMLImageAccessible(nsIDOMNode* aDomNode, nsIImageFrame *imageFrame, nsIWeakReference* aShell);
  NS_IMETHOD GetAccName(nsAWritableString& _retval); 
  NS_IMETHOD GetAccState(PRUint32 *_retval); 
  NS_IMETHOD GetAccRole(PRUint32 *_retval); 
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);

protected:
  nsIAccessible *CreateAreaAccessible(PRUint32 areaNum);
  nsCOMPtr<nsIDOMHTMLMapElement> mMapElement;
};

#endif  
