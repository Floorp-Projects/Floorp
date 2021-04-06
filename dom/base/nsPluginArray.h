/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginArray_h___
#define nsPluginArray_h___

#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsPIDOMWindowInner;
class nsPluginElement;
class nsMimeType;

namespace mozilla::dom {
enum class CallerType : uint32_t;
}  // namespace mozilla::dom

/**
 * Array class backing HTML's navigator.plugins.  This array is always empty.
 */
class nsPluginArray final : public nsSupportsWeakReference,
                            public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPluginArray)

  explicit nsPluginArray(nsPIDOMWindowInner* aWindow);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // PluginArray WebIDL methods
  void Refresh(bool aReloadDocuments) {}

  nsPluginElement* Item(uint32_t aIndex, mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  nsPluginElement* NamedItem(const nsAString& aName,
                             mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  uint32_t Length(mozilla::dom::CallerType aCallerType) { return 0; }

  nsPluginElement* IndexedGetter(uint32_t aIndex, bool& aFound,
                                 mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  nsPluginElement* NamedGetter(const nsAString& aName, bool& aFound,
                               mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  void GetSupportedNames(nsTArray<nsString>& aRetval,
                         mozilla::dom::CallerType aCallerType) {}

 private:
  virtual ~nsPluginArray();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

/**
 * Plugin class backing entries in HTML's navigator.plugins array.
 * Currently, these cannot be constructed.
 */
class nsPluginElement final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPluginElement)

  nsPluginElement() = delete;

  nsPIDOMWindowInner* GetParentObject() const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
    return nullptr;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return nullptr;
  }

  // Plugin WebIDL methods
  void GetDescription(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
  }

  void GetFilename(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
  }

  void GetVersion(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
  }

  void GetName(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
  }

  nsMimeType* Item(uint32_t index) {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return nullptr;
  }

  nsMimeType* NamedItem(const nsAString& name) {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return nullptr;
  }
  uint32_t Length() {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return 0;
  }

  nsMimeType* IndexedGetter(uint32_t index, bool& found) {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return nullptr;
  }

  nsMimeType* NamedGetter(const nsAString& name, bool& found) {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
    return nullptr;
  }

  void GetSupportedNames(nsTArray<nsString>& retval) {
    MOZ_ASSERT_UNREACHABLE("nsPluginElement can not exist");
  }

 protected:
  virtual ~nsPluginElement() = default;
};

#endif /* nsPluginArray_h___ */
