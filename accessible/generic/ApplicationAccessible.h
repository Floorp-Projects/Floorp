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

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual void Shutdown();
  virtual nsIntRect Bounds() const MOZ_OVERRIDE;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() MOZ_OVERRIDE;
  virtual GroupPos GroupPosition();
  virtual ENameValueFlag Name(nsString& aName);
  virtual void ApplyARIAState(uint64_t* aState) const;
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole() MOZ_OVERRIDE;
  virtual uint64_t State() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;

  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual Accessible* FocusedChild();

  virtual void InvalidateChildren();

  // ActionAccessible
  virtual KeyBinding AccessKey() const;

  // ApplicationAccessible
  void AppName(nsAString& aName) const
  {
    nsAutoCString cname;
    mAppInfo->GetName(cname);
    AppendUTF8toUTF16(cname, aName);
  }

  void AppVersion(nsAString& aVersion) const
  {
    nsAutoCString cversion;
    mAppInfo->GetVersion(cversion);
    AppendUTF8toUTF16(cversion, aVersion);
  }

  void PlatformName(nsAString& aName) const
  {
    aName.AssignLiteral("Gecko");
  }

  void PlatformVersion(nsAString& aVersion) const
  {
    nsAutoCString cversion;
    mAppInfo->GetPlatformVersion(cversion);
    AppendUTF8toUTF16(cversion, aVersion);
  }

protected:
  virtual ~ApplicationAccessible() {}

  // Accessible
  virtual void CacheChildren();
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult *aError = nullptr) const;

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

