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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

  NS_IMETHOD SetDefaultContent(nsIContent* aDefaultContent)=0;
  NS_IMETHOD GetDefaultContent(nsIContent** aDefaultContent)=0;

  NS_IMETHOD SetDefaultContentTemplate(nsIContent* aDefaultContent)=0;
  NS_IMETHOD GetDefaultContentTemplate(nsIContent** aDefaultContent)=0;

  NS_IMETHOD AddChild(nsIContent* aChildElement)=0;
  NS_IMETHOD InsertChildAt(PRInt32 aIndex, nsIContent* aChildElement)=0;
  NS_IMETHOD RemoveChild(nsIContent* aChildElement)=0;

  NS_IMETHOD ChildCount(PRUint32* aResult)=0;

  NS_IMETHOD ChildAt(PRUint32 aIndex, nsIContent** aResult)=0;

  NS_IMETHOD Matches(nsIContent* aContent, PRUint32 aIndex, PRBool* aResult)=0;
};

extern nsresult
NS_NewXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIContent* aDefContent,
                        nsIXBLInsertionPoint** aResult);

#endif // nsIXBLInsertionPoint_h__
