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
#include "nsIWeakReferenceUtils.h"
#include "nsAutoPtr.h"

class nsPIDOMWindow;
class nsMimeType;
class nsPluginElement;

class nsMimeTypeArray MOZ_FINAL : public nsISupports,
                                  public nsWrapperCache
{
public:
  nsMimeTypeArray(nsWeakPtr aWindow);
  virtual ~nsMimeTypeArray();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsMimeTypeArray)

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  void Refresh();

  // MimeTypeArray WebIDL methods
  nsMimeType* Item(uint32_t index);
  nsMimeType* NamedItem(const nsAString& name);
  nsMimeType* IndexedGetter(uint32_t index, bool &found);
  nsMimeType* NamedGetter(const nsAString& name, bool &found);
  uint32_t Length();
  void GetSupportedNames(nsTArray< nsString >& retval);

protected:
  void EnsureMimeTypes();
  void Clear();

  nsWeakPtr mWindow;

  // mMimeTypes contains all mime types handled by plugins followed by
  // any other mime types that we handle internally and have been
  // looked up before.
  nsTArray<nsRefPtr<nsMimeType> > mMimeTypes;

  // mPluginMimeTypeCount is the number of plugin mime types that we
  // have in mMimeTypes. The plugin mime types are always at the
  // beginning of the list.
  uint32_t mPluginMimeTypeCount;
};

class nsMimeType MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsMimeType)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsMimeType)

  nsMimeType(nsWeakPtr aWindow, nsPluginElement* aPluginElement,
             uint32_t aPluginTagMimeIndex, const nsAString& aMimeType);
  nsMimeType(nsWeakPtr aWindow, const nsAString& aMimeType);
  virtual ~nsMimeType();

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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
  nsWeakPtr mWindow;

  // Strong reference to the active plugin, if any. Note that this
  // creates an explicit reference cycle through the plugin element's
  // mimetype array. We rely on the cycle collector to break this
  // cycle.
  nsRefPtr<nsPluginElement> mPluginElement;
  uint32_t mPluginTagMimeIndex;
  nsString mType;
};

#endif /* nsMimeTypeArray_h___ */
