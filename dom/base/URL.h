/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef URL_h___
#define URL_h___

#include "nscore.h"
#include "mozilla/dom/URLBinding.h"

class nsIDOMBlob;

namespace mozilla {
namespace dom {

class URL MOZ_FINAL
{
public:
  // WebIDL methods
  static void CreateObjectURL(nsISupports* aGlobal, nsIDOMBlob* aBlob,
                              const objectURLOptions& aOptions,
                              nsAString& aResult,
                              ErrorResult& aError);
  static void RevokeObjectURL(nsISupports* aGlobal, const nsAString& aURL);
};

}
}

#endif /* URL_h___ */
