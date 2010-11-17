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
 *   John Gaunt (jgaunt@netscape.com)
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

#ifndef _nsBaseWidgetAccessible_H_
#define _nsBaseWidgetAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsHyperTextAccessibleWrap.h"
#include "nsIContent.h"

/**
  * This file contains a number of classes that are used as base
  *  classes for the different accessibility implementations of
  *  the HTML and XUL widget sets.  --jgaunt
  */

/** 
  * Leaf version of DOM Accessible -- has no children
  */
class nsLeafAccessible : public nsAccessibleWrap
{
public:
  using nsAccessible::GetChildAtPoint;

  nsLeafAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsAccessible
  virtual nsresult GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   PRBool aDeepestChild,
                                   nsIAccessible **aChild);

protected:

  // nsAccessible
  virtual void CacheChildren();
};

/**
 * Used for text or image accessible nodes contained by link accessibles or
 * accessibles for nodes with registered click event handler. It knows how to
 * report the state of the host link (traveled or not) and can activate (click)
 * the host accessible programmatically.
 */
class nsLinkableAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Jump = 0 };

  nsLinkableAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetValue(nsAString& _retval);
  NS_IMETHOD TakeFocus();
  NS_IMETHOD GetKeyboardShortcut(nsAString& _retval);

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // HyperLinkAccessible
  virtual already_AddRefed<nsIURI> GetAnchorURI(PRUint32 aAnchorIndex);

protected:
  // nsAccessible
  virtual void BindToParent(nsAccessible* aParent, PRUint32 aIndexInParent);

  // nsLinkableAccessible

  /**
   * Return an accessible for cached action node.
   */
  nsAccessible *GetActionAccessible() const;

  nsCOMPtr<nsIContent> mActionContent;
  PRPackedBool mIsLink;
  PRPackedBool mIsOnclick;
};

/**
 * A simple accessible that gets its enumerated role passed into constructor.
 */ 
class nsEnumRoleAccessible : public nsAccessibleWrap
{
public:
  nsEnumRoleAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                       PRUint32 aRole);
  virtual ~nsEnumRoleAccessible() { }

  NS_DECL_ISUPPORTS_INHERITED

  // nsAccessible
  virtual PRUint32 NativeRole();

protected:
  PRUint32 mRole;
};

#endif  
