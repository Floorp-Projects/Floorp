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

nsresult ImportModule(const char* aURI, const nsIID& aIID, void** aResult);

}  // namespace loader
}  // namespace mozilla

class MOZ_STACK_CLASS nsImportModule final : public nsCOMPtr_helper {
 public:
  nsImportModule(const char* aURI, nsresult* aErrorPtr)
      : mURI(aURI), mErrorPtr(aErrorPtr) {}

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const override {
    nsresult rv = ::mozilla::loader::ImportModule(mURI, aIID, aResult);
    if (mErrorPtr) {
      *mErrorPtr = rv;
    }
    return rv;
  }

 private:
  const char* mURI;
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
 */

inline nsImportModule do_ImportModule(const char* aURI) {
  return {aURI, nullptr};
}

inline nsImportModule do_ImportModule(const char* aURI, nsresult* aRv) {
  return {aURI, aRv};
}

#endif  // defined nsImportModule_h
