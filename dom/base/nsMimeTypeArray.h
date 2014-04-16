/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMimeTypeArray_h___
#define nsMimeTypeArray_h___

#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsPIDOMWindow.h"

class nsMimeType;
class nsPluginElement;

class nsMimeTypeArray MOZ_FINAL : public nsISupports,
                                  public nsWrapperCache
{
public:
  nsMimeTypeArray(nsPIDOMWindow* aWindow);
  virtual ~nsMimeTypeArray();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsMimeTypeArray)

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void Refresh();

  // MimeTypeArray WebIDL methods
  nsMimeType* Item(uint32_t index);
  nsMimeType* NamedItem(const nsAString& name);
  nsMimeType* IndexedGetter(uint32_t index, bool &found);
  nsMimeType* NamedGetter(const nsAString& name, bool &found);
  bool NameIsEnumerable(const nsAString& name);
  uint32_t Length();
  void GetSupportedNames(unsigned, nsTArray< nsString >& retval);

protected:
  void EnsurePluginMimeTypes();
  void Clear();

  nsCOMPtr<nsPIDOMWindow> mWindow;

  // mMimeTypes contains MIME types handled by non-hidden plugins, those
  // popular plugins that must be exposed in navigator.plugins enumeration to
  // avoid breaking web content. Likewise, mMimeTypes are exposed in
  // navigator.mimeTypes enumeration.
  nsTArray<nsRefPtr<nsMimeType> > mMimeTypes;

  // mHiddenMimeTypes contains MIME types handled by plugins hidden from
  // navigator.plugins enumeration or by an OS PreferredApplicationHandler.
  // mHiddenMimeTypes are hidden from navigator.mimeTypes enumeration.
  nsTArray<nsRefPtr<nsMimeType> > mHiddenMimeTypes;
};

class nsMimeType MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsMimeType)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsMimeType)

  nsMimeType(nsPIDOMWindow* aWindow, nsPluginElement* aPluginElement,
             uint32_t aPluginTagMimeIndex, const nsAString& aMimeType);
  nsMimeType(nsPIDOMWindow* aWindow, const nsAString& aMimeType);
  virtual ~nsMimeType();

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  const nsString& Type() const
  {
    return mType;
  }

  // MimeType WebIDL methods
  void GetDescription(nsString& retval) const;
  nsPluginElement *GetEnabledPlugin() const;
  void GetSuffixes(nsString& retval) const;
  void GetType(nsString& retval) const;

protected:
  nsCOMPtr<nsPIDOMWindow> mWindow;

  // Strong reference to the active plugin, if any. Note that this
  // creates an explicit reference cycle through the plugin element's
  // mimetype array. We rely on the cycle collector to break this
  // cycle.
  nsRefPtr<nsPluginElement> mPluginElement;
  uint32_t mPluginTagMimeIndex;
  nsString mType;
};

#endif /* nsMimeTypeArray_h___ */
