/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIXBLInsertionPoint_h__
#define nsIXBLInsertionPoint_h__

class nsIContent;

// {3DAE842A-9436-4920-AC42-28C54C859066}
#define NS_IXBLINSERTIONPOINT_IID \
{ 0x3dae842a, 0x9436, 0x4920, { 0xac, 0x42, 0x28, 0xc5, 0x4c, 0x85, 0x90, 0x66 } }

class nsIXBLInsertionPoint : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLINSERTIONPOINT_IID; return iid; }

  NS_IMETHOD GetInsertionParent(nsIContent** aParentElement)=0;
  NS_IMETHOD GetInsertionIndex(PRInt32* aResult)=0;
  NS_IMETHOD AddChild(nsIContent* aChildElement)=0;
  NS_IMETHOD RemoveChild(nsIContent* aChildElement)=0;

  NS_IMETHOD ChildCount(PRUint32* aResult)=0;

  NS_IMETHOD ChildAt(PRUint32 aIndex, nsIContent** aResult)=0;

  NS_IMETHOD Matches(nsIContent* aContent, PRUint32 aIndex, PRBool* aResult)=0;
};

extern nsresult
NS_NewXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIXBLInsertionPoint** aResult);

#endif // nsIXBLInsertionPoint_h__
