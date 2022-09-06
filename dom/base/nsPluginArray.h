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
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Array.h"

class nsPIDOMWindowInner;
class nsPluginElement;
class nsMimeTypeArray;
class nsMimeType;

/**
 * Array class backing HTML's navigator.plugins.  This always holds references
 * to the hard-coded set of PDF plugins defined by HTML but it only consults
 * them if "pdfjs.disabled" is false.  There is never more than one of these
 * per DOM window.
 */
class nsPluginArray final : public nsSupportsWeakReference,
                            public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsPluginArray)

  explicit nsPluginArray(nsPIDOMWindowInner* aWindow);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsMimeTypeArray* MimeTypeArray() { return mMimeTypeArray; }

  // PluginArray WebIDL methods
  uint32_t Length() { return ForceNoPlugins() ? 0 : ArrayLength(mPlugins); }

  nsPluginElement* Item(uint32_t aIndex) {
    bool unused;
    return IndexedGetter(aIndex, unused);
  }

  nsPluginElement* NamedItem(const nsAString& aName) {
    bool unused;
    return NamedGetter(aName, unused);
  }

  nsPluginElement* IndexedGetter(uint32_t aIndex, bool& aFound);

  nsPluginElement* NamedGetter(const nsAString& aName, bool& aFound);

  void GetSupportedNames(nsTArray<nsString>& aRetval);

  void Refresh() {}

 private:
  virtual ~nsPluginArray();

  bool ForceNoPlugins();

  RefPtr<nsMimeTypeArray> mMimeTypeArray;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  mozilla::Array<RefPtr<nsPluginElement>, 5> mPlugins;
};

/**
 * Plugin class backing entries in HTML's navigator.plugins array.  There is
 * a fixed set of these, as defined by HTML.
 */
class nsPluginElement final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsPluginElement)

  explicit nsPluginElement(nsPluginArray* aPluginArray,
                           nsPIDOMWindowInner* aWindow, const nsAString& aName);

  nsPluginArray* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Plugin WebIDL methods
  void GetDescription(nsString& retval) const { retval = kDescription; }

  void GetFilename(nsString& retval) const { retval = kFilename; }

  void GetName(nsString& retval) const { retval = mName; }
  const nsString& Name() const { return mName; }

  nsMimeType* Item(uint32_t index) {
    bool unused;
    return IndexedGetter(index, unused);
  }

  nsMimeType* NamedItem(const nsAString& name) {
    bool unused;
    return NamedGetter(name, unused);
  }

  uint32_t Length();

  nsMimeType* IndexedGetter(uint32_t index, bool& found);

  nsMimeType* NamedGetter(const nsAString& name, bool& found);

  void GetSupportedNames(nsTArray<nsString>& retval);

 protected:
  virtual ~nsPluginElement() = default;

  nsMimeTypeArray* MimeTypeArray() { return mPluginArray->MimeTypeArray(); }

  static constexpr nsLiteralString kDescription =
      u"Portable Document Format"_ns;
  static constexpr nsLiteralString kFilename = u"internal-pdf-viewer"_ns;

  // Note that this creates an explicit reference cycle:
  //
  // nsPluginElement -> nsPluginArray -> nsPluginElement
  //
  // We rely on the cycle collector to break this cycle.
  RefPtr<nsPluginArray> mPluginArray;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mName;
};

#endif /* nsPluginArray_h___ */
