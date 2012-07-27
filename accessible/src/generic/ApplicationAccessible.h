/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ApplicationAccessible_h__
#define mozilla_a11y_ApplicationAccessible_h__

#include "AccessibleWrap.h"
#include "nsIAccessibleApplication.h"

#include "nsIMutableArray.h"
#include "nsIXULAppInfo.h"

namespace mozilla {
namespace a11y {

/**
 * ApplicationAccessible is for the whole application of Mozilla.
 * Only one instance of ApplicationAccessible exists for one Mozilla instance.
 * And this one should be created when Mozilla Startup (if accessibility
 * feature has been enabled) and destroyed when Mozilla Shutdown.
 *
 * All the accessibility objects for toplevel windows are direct children of
 * the ApplicationAccessible instance.
 */

class ApplicationAccessible : public AccessibleWrap,
                             public nsIAccessibleApplication
{
public:

  ApplicationAccessible();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetDOMNode(nsIDOMNode** aDOMNode);
  NS_IMETHOD GetDocument(nsIAccessibleDocument** aDocument);
  NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument);
  NS_IMETHOD ScrollTo(PRUint32 aScrollType);
  NS_IMETHOD ScrollToPoint(PRUint32 aCoordinateType, PRInt32 aX, PRInt32 aY);
  NS_IMETHOD GetLanguage(nsAString& aLanguage);
  NS_IMETHOD GetParent(nsIAccessible **aParent);
  NS_IMETHOD GetNextSibling(nsIAccessible **aNextSibling);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **aPreviousSibling);
  NS_IMETHOD GetAttributes(nsIPersistentProperties **aAttributes);
  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);
  NS_IMETHOD SetSelected(bool aIsSelected);
  NS_IMETHOD TakeSelection();
  NS_IMETHOD TakeFocus();
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString &aName);
  NS_IMETHOD GetActionDescription(PRUint8 aIndex, nsAString &aDescription);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleApplication
  NS_DECL_NSIACCESSIBLEAPPLICATION

  // nsAccessNode
  virtual void Init();
  virtual void Shutdown();
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual GroupPos GroupPosition();
  virtual ENameValueFlag Name(nsString& aName);
  virtual void ApplyARIAState(PRUint64* aState) const;
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 State();
  virtual PRUint64 NativeState();
  virtual Relation RelationByType(PRUint32 aRelType);

  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual Accessible* FocusedChild();

  virtual void InvalidateChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();
  virtual KeyBinding AccessKey() const;

protected:

  // Accessible
  virtual void CacheChildren();
  virtual Accessible* GetSiblingAtOffset(PRInt32 aOffset,
                                         nsresult *aError = nullptr) const;

private:
  nsCOMPtr<nsIXULAppInfo> mAppInfo;
};

} // namespace a11y
} // namespace mozilla

#endif

