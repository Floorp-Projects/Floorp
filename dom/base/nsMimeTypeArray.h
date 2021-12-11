/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMimeTypeArray_h___
#define nsMimeTypeArray_h___

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsMimeType;
class nsPluginElement;

namespace mozilla::dom {
enum class CallerType : uint32_t;
}  // namespace mozilla::dom

/**
 * Array class backing HTML's navigator.mimeTypes.  This array is always empty.
 */
class nsMimeTypeArray final : public nsISupports, public nsWrapperCache {
 public:
  explicit nsMimeTypeArray(nsPIDOMWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsMimeTypeArray)

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void Refresh() {}

  // MimeTypeArray WebIDL methods
  nsMimeType* Item(uint32_t index, mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  nsMimeType* NamedItem(const nsAString& name,
                        mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  nsMimeType* IndexedGetter(uint32_t index, bool& found,
                            mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  nsMimeType* NamedGetter(const nsAString& name, bool& found,
                          mozilla::dom::CallerType aCallerType) {
    return nullptr;
  }

  uint32_t Length(mozilla::dom::CallerType aCallerType) { return 0; }

  void GetSupportedNames(nsTArray<nsString>& retval,
                         mozilla::dom::CallerType aCallerType) {}

 protected:
  virtual ~nsMimeTypeArray();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

/**
 * Mime type class backing entries in HTML's navigator.mimeTypes array.
 * Currently, these cannot be constructed.
 */
class nsMimeType final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsMimeType)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsMimeType)

  // Never constructed.
  nsMimeType() = delete;

  nsPIDOMWindowInner* GetParentObject() const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
    return nullptr;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
    return nullptr;
  }

  // MimeType WebIDL methods
  void GetDescription(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
  }

  nsPluginElement* GetEnabledPlugin() const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
    return nullptr;
  }

  void GetSuffixes(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
  }

  void GetType(nsString& retval) const {
    MOZ_ASSERT_UNREACHABLE("nsMimeType can not exist");
  }

 protected:
  virtual ~nsMimeType() = default;
};

#endif /* nsMimeTypeArray_h___ */
