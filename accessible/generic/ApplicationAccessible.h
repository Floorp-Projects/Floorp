/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ApplicationAccessible_h__
#define mozilla_a11y_ApplicationAccessible_h__

#include "AccessibleWrap.h"

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

class ApplicationAccessible : public AccessibleWrap
{
public:

  ApplicationAccessible();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ApplicationAccessible, AccessibleWrap)

  // Accessible
  virtual void Shutdown() override;
  virtual nsIntRect Bounds() const override;
  virtual nsRect BoundsInAppUnits() const override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;
  virtual GroupPos GroupPosition() override;
  virtual ENameValueFlag Name(nsString& aName) const override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual void Description(nsString& aDescription) override;
  virtual void Value(nsString& aValue) const override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t State() override;
  virtual uint64_t NativeState() const override;
  virtual Relation RelationByType(RelationType aType) const override;

  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) override;
  virtual Accessible* FocusedChild() override;

  // ActionAccessible
  virtual KeyBinding AccessKey() const override;

  // ApplicationAccessible
  void Init();

  void AppName(nsAString& aName) const
  {
    MOZ_ASSERT(mAppInfo, "no application info");

    if (mAppInfo) {
      nsAutoCString cname;
      mAppInfo->GetName(cname);
      AppendUTF8toUTF16(cname, aName);
    }
  }

  void AppVersion(nsAString& aVersion) const
  {
    MOZ_ASSERT(mAppInfo, "no application info");

    if (mAppInfo) {
      nsAutoCString cversion;
      mAppInfo->GetVersion(cversion);
      AppendUTF8toUTF16(cversion, aVersion);
    }
  }

  void PlatformName(nsAString& aName) const
  {
    aName.AssignLiteral("Gecko");
  }

  void PlatformVersion(nsAString& aVersion) const
  {
    MOZ_ASSERT(mAppInfo, "no application info");

    if (mAppInfo) {
      nsAutoCString cversion;
      mAppInfo->GetPlatformVersion(cversion);
      AppendUTF8toUTF16(cversion, aVersion);
    }
  }

protected:
  virtual ~ApplicationAccessible() {}

  // Accessible
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult *aError = nullptr) const override;

private:
  nsCOMPtr<nsIXULAppInfo> mAppInfo;
};

inline ApplicationAccessible*
Accessible::AsApplication()
{
  return IsApplication() ? static_cast<ApplicationAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

