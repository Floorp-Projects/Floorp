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
 *   Author: John Gaunt (jgaunt@netscape.com)
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

#ifndef _nsXULTabAccessible_H_
#define _nsXULTabAccessible_H_

// NOTE: alphabetically ordered
#include "nsBaseWidgetAccessible.h"
#include "nsXULSelectAccessible.h"

/** An individual tab */
class nsXULTabAccessible : public nsLeafAccessible
{
public:
  enum { eAction_Switch = 0 };

  nsXULTabAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
};

/** 
  * Contains a tabs object and a tabPanels object. A complete
  *    entity with relationships between tabs and content to
  *    be displayed in the tabpanels object
  */
class nsXULTabBoxAccessible : public nsAccessibleWrap
{
public:
  nsXULTabBoxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  //NS_IMETHOD GetChildCount(PRInt32 *_retval); // aaronl remove this?
};

/** 
  * Represents the content area associated with the tabs object (when
  *   used together)
  */
class nsXULTabPanelsAccessible : public nsAccessibleWrap
{
public:
  nsXULTabPanelsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetName(nsAString& _retval); 

protected:
  nsresult GetAccPluginChild(nsIAccessible **_retval);

  // data members
  nsCOMPtr<nsIDOMNode> mGParentDOMNode;
  nsCOMPtr<nsIDOMNode> mParentDOMNode;
};

/** merely a container of tab obejcts */
class nsXULTabsAccessible : public nsXULSelectableAccessible
{
public:
  nsXULTabsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetValue(nsAString& _retval);
  NS_IMETHOD GetName(nsAString& _retval);
};

#endif
