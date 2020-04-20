/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsImportModule_h
#define nsImportModule_h

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace loader {

nsresult ImportModule(const char* aURI, const char* aExportName,
                      const nsIID& aIID, void** aResult);

}  // namespace loader
}  // namespace mozilla

class MOZ_STACK_CLASS nsImportModule final : public nsCOMPtr_helper {
 public:
  nsImportModule(const char* aURI, const char* aExportName, nsresult* aErrorPtr)
      : mURI(aURI), mExportName(aExportName), mErrorPtr(aErrorPtr) {}

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const override {
    nsresult rv =
        ::mozilla::loader::ImportModule(mURI, mExportName, aIID, aResult);
    if (mErrorPtr) {
      *mErrorPtr = rv;
    }
    return rv;
  }

 private:
  const char* mURI;
  const char* mExportName;
  nsresult* mErrorPtr;
};

/**
 * These helpers make it considerably easier for C++ code to import a JS module
 * and wrap it in an appropriately-defined XPIDL interface for its exports.
 * Typical usage is something like:
 *
 * Foo.jsm:
 *
 *   var EXPORTED_SYMBOLS = ["foo"];
 *
 *   function foo(bar) {
 *     return bar.toString();
 *   }
 *
 * mozIFoo.idl:
 *
 *   interface mozIFoo : nsISupports {
 *     AString foo(double meh);
 *   }
 *
 * Thing.cpp:
 *
 *   nsCOMPtr<mozIFoo> foo = do_ImportModule(
 *     "resource://meh/Foo.jsm");
 *
 *   MOZ_TRY(foo->Foo(42));
 *
 * For JS modules which export all fields within a single named object, a second
 * argument can be passed naming that object.
 *
 * Foo.jsm:
 *
 *   var EXPORTED_SYMBOLS = ["Foo"];
 *
 *   var Foo = {
 *     function foo(bar) {
 *       return bar.toString();
 *     }
 *   };
 *
 * Thing.cpp:
 *
 *   nsCOMPtr<mozIFoo> foo = do_ImportModule(
 *       "resource:://meh/Foo.jsm", "Foo");
 */

template <size_t N>
inline nsImportModule do_ImportModule(const char (&aURI)[N]) {
  return {aURI, nullptr, nullptr};
}

template <size_t N>
inline nsImportModule do_ImportModule(const char (&aURI)[N], nsresult* aRv) {
  return {aURI, nullptr, aRv};
}

template <size_t N, size_t N2>
inline nsImportModule do_ImportModule(const char (&aURI)[N],
                                      const char (&aExportName)[N2]) {
  return {aURI, aExportName, nullptr};
}

template <size_t N, size_t N2>
inline nsImportModule do_ImportModule(const char (&aURI)[N],
                                      const char (&aExportName)[N2],
                                      nsresult* aRv) {
  return {aURI, aExportName, aRv};
}

#endif  // defined nsImportModule_h
