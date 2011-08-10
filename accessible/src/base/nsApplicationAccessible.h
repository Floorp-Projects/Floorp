/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin <bolian.yin@sun.com>
 *   Ginn Chen <ginn.chen@sun.com>
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_APPLICATION_ACCESSIBLE_H__
#define __NS_APPLICATION_ACCESSIBLE_H__

#include "nsAccessibleWrap.h"
#include "nsIAccessibleApplication.h"

#include "nsIMutableArray.h"
#include "nsIXULAppInfo.h"

/**
 * nsApplicationAccessible is for the whole application of Mozilla.
 * Only one instance of nsApplicationAccessible exists for one Mozilla instance.
 * And this one should be created when Mozilla Startup (if accessibility
 * feature has been enabled) and destroyed when Mozilla Shutdown.
 *
 * All the accessibility objects for toplevel windows are direct children of
 * the nsApplicationAccessible instance.
 */

class nsApplicationAccessible: public nsAccessibleWrap,
                               public nsIAccessibleApplication
{
public:

  nsApplicationAccessible();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessNode
  NS_SCRIPTABLE NS_IMETHOD GetDOMNode(nsIDOMNode** aDOMNode);
  NS_SCRIPTABLE NS_IMETHOD GetDocument(nsIAccessibleDocument** aDocument);
  NS_SCRIPTABLE NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument);
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML);
  NS_SCRIPTABLE NS_IMETHOD ScrollTo(PRUint32 aScrollType);
  NS_SCRIPTABLE NS_IMETHOD ScrollToPoint(PRUint32 aCoordinateType, PRInt32 aX, PRInt32 aY);
  NS_SCRIPTABLE NS_IMETHOD GetComputedStyleValue(const nsAString& aPseudoElt,
                                                 const nsAString& aPropertyName,
                                                 nsAString& aValue NS_OUTPARAM);
  NS_SCRIPTABLE NS_IMETHOD GetComputedStyleCSSValue(const nsAString& aPseudoElt,
                                                    const nsAString& aPropertyName,
                                                    nsIDOMCSSPrimitiveValue** aValue NS_OUTPARAM);
  NS_SCRIPTABLE NS_IMETHOD GetLanguage(nsAString& aLanguage);

  // nsIAccessible
  NS_IMETHOD GetParent(nsIAccessible **aParent);
  NS_IMETHOD GetNextSibling(nsIAccessible **aNextSibling);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **aPreviousSibling);
  NS_IMETHOD GetName(nsAString &aName);
  NS_IMETHOD GetValue(nsAString &aValue);
  NS_IMETHOD GetAttributes(nsIPersistentProperties **aAttributes);
  NS_IMETHOD GroupPosition(PRInt32 *aGroupLevel, PRInt32 *aSimilarItemsInGroup,
                           PRInt32 *aPositionInGroup);
  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);
  NS_IMETHOD SetSelected(PRBool aIsSelected);
  NS_IMETHOD TakeSelection();
  NS_IMETHOD TakeFocus();
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString &aName);
  NS_IMETHOD GetActionDescription(PRUint8 aIndex, nsAString &aDescription);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleApplication
  NS_DECL_NSIACCESSIBLEAPPLICATION

  // nsAccessNode
  virtual bool IsDefunct() const;
  virtual PRBool Init();
  virtual void Shutdown();
  virtual bool IsPrimaryForNode() const;

  // nsAccessible
  virtual void ApplyARIAState(PRUint64* aState);
  virtual void Description(nsString& aDescription);
  virtual PRUint32 NativeRole();
  virtual PRUint64 State();
  virtual PRUint64 NativeState();

  virtual nsAccessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     EWhichChildAtPoint aWhichChild);
  virtual nsAccessible* FocusedChild();

  virtual void InvalidateChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();
  virtual KeyBinding AccessKey() const;

protected:

  // nsAccessible
  virtual void CacheChildren();
  virtual nsAccessible* GetSiblingAtOffset(PRInt32 aOffset,
                                           nsresult *aError = nsnull) const;

private:
  nsCOMPtr<nsIXULAppInfo> mAppInfo;
};

#endif

