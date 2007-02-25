/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _nsXULMenuAccessible_H_
#define _nsXULMenuAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsAccessibleTreeWalker.h"
#include "nsIAccessibleSelectable.h"
#include "nsIDOMXULSelectCntrlEl.h"

/*
 * The basic implemetation of nsIAccessibleSelectable.
 */
class nsXULSelectableAccessible : public nsAccessibleWrap
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE

  nsXULSelectableAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULSelectableAccessible() {}
  NS_IMETHOD Shutdown();

protected:
  nsresult ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState);
  nsresult AppendFlatStringFromSubtree(nsIContent *aContent, nsAString *aFlatString)
    { return NS_OK; }  // Overrides base impl in nsAccessible

  // nsIDOMXULMultiSelectControlElement inherits from this, so we'll always have
  // one of these if the widget is valid and not defunct
  nsCOMPtr<nsIDOMXULSelectControlElement> mSelectControl;
};

/* Accessible for supporting XUL menus
 */

class nsXULMenuitemAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  nsXULMenuitemAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD Init();
  NS_IMETHOD GetName(nsAString& _retval); 
  NS_IMETHOD GetDescription(nsAString& aDescription); 
  NS_IMETHOD GetKeyboardShortcut(nsAString& _retval);
  NS_IMETHOD GetKeyBinding(nsAString& _retval);
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren);
};

class nsXULMenuSeparatorAccessible : public nsXULMenuitemAccessible
{
public:
  nsXULMenuSeparatorAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& _retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
};

class nsXULMenupopupAccessible : public nsXULSelectableAccessible
{
public:
  nsXULMenupopupAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& _retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  static already_AddRefed<nsIDOMNode> FindInNodeList(nsIDOMNodeList *aNodeList,
                                                     nsIAtom *aAtom, PRUint32 aNameSpaceID);
  static void GenerateMenu(nsIDOMNode *aNode);
};

class nsXULMenubarAccessible : public nsAccessibleWrap
{
public:
  nsXULMenubarAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& _retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetRole(PRUint32 *_retval); 
};

#endif  
