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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#ifndef nsXBLInsertionPoint_h__
#define nsXBLInsertionPoint_h__

#include "nsIXBLInsertionPoint.h"
#include "nsISupportsArray.h"

class nsXBLInsertionPoint : public nsIXBLInsertionPoint
{
public:
  nsXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex);
  virtual ~nsXBLInsertionPoint();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetInsertionParent(nsIContent** aParentElement);
  NS_IMETHOD GetInsertionIndex(PRInt32* aIndex);

  NS_IMETHOD AddChild(nsIContent* aChildElement);
  NS_IMETHOD RemoveChild(nsIContent* aChildElement);

  NS_IMETHOD ChildCount(PRUint32* aResult);

  NS_IMETHOD ChildAt(PRUint32 aIndex, nsIContent** aResult);

  NS_IMETHOD Matches(nsIContent* aContent, PRUint32 aIndex, PRBool* aResult);

protected:
  nsIContent* mParentElement;            // This ref is weak.  The parent of the <children> element.
  PRInt32 mIndex;                        // The index of this insertion point. -1 is a pseudo-point.
  nsCOMPtr<nsISupportsArray> mElements;  // An array of elements present at the insertion point.
};

extern nsresult
NS_NewXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIXBLInsertionPoint** aResult);

#endif
