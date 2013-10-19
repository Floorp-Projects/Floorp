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
  NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument);
  NS_IMETHOD ScrollTo(uint32_t aScrollType);
  NS_IMETHOD ScrollToPoint(uint32_t aCoordinateType, int32_t aX, int32_t aY);
  NS_IMETHOD GetLanguage(nsAString& aLanguage);
  NS_IMETHOD GetParent(nsIAccessible **aParent);
  NS_IMETHOD GetNextSibling(nsIAccessible **aNextSibling);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **aPreviousSibling);
  NS_IMETHOD GetBounds(int32_t *aX, int32_t *aY,
                       int32_t *aWidth, int32_t *aHeight);
  NS_IMETHOD SetSelected(bool aIsSelected);
  NS_IMETHOD TakeSelection();
  NS_IMETHOD TakeFocus();
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString &aName);
  NS_IMETHOD GetActionDescription(uint8_t aIndex, nsAString &aDescription);
  NS_IMETHOD DoAction(uint8_t aIndex);

  // nsIAccessibleApplication
  NS_DECL_NSIACCESSIBLEAPPLICATION

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() MOZ_OVERRIDE;
  virtual GroupPos GroupPosition();
  virtual ENameValueFlag Name(nsString& aName);
  virtual void ApplyARIAState(uint64_t* aState) const;
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t State();
  virtual uint64_t NativeState();
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;

  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual Accessible* FocusedChild();

  virtual void InvalidateChildren();

  // ActionAccessible
  virtual uint8_t ActionCount();
  virtual KeyBinding AccessKey() const;

protected:

  // Accessible
  virtual void CacheChildren();
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult *aError = nullptr) const;

private:
  nsCOMPtr<nsIXULAppInfo> mAppInfo;
};

} // namespace a11y
} // namespace mozilla

#endif

