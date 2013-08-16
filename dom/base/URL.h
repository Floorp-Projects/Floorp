/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef URL_h___
#define URL_h___

#include "nscore.h"
#include "nsString.h"

class nsIDOMBlob;
class nsISupports;

namespace mozilla {

class ErrorResult;
class DOMMediaStream;

namespace dom {

class MediaSource;
class GlobalObject;
struct objectURLOptions;

class URL MOZ_FINAL
{
public:
  // WebIDL methods
  static void CreateObjectURL(const GlobalObject& aGlobal, nsIDOMBlob* aBlob,
                              const objectURLOptions& aOptions,
                              nsString& aResult,
                              ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              DOMMediaStream& aStream,
                              const mozilla::dom::objectURLOptions& aOptions,
                              nsString& aResult,
                              mozilla::ErrorResult& aError);
  static void CreateObjectURL(const GlobalObject& aGlobal,
                              MediaSource& aSource,
                              const objectURLOptions& aOptions,
                              nsString& aResult,
                              mozilla::ErrorResult& aError);
  static void RevokeObjectURL(const GlobalObject& aGlobal,
                              const nsAString& aURL);

private:
  static void CreateObjectURLInternal(nsISupports* aGlobal, nsISupports* aObject,
                                      const nsACString& aScheme,
                                      const mozilla::dom::objectURLOptions& aOptions,
                                      nsString& aResult,
                                      mozilla::ErrorResult& aError);
};

}
}

#endif /* URL_h___ */
