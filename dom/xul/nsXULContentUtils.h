/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A package of routines shared by the XUL content code.

 */

#ifndef nsXULContentUtils_h__
#define nsXULContentUtils_h__

#include "nsISupports.h"

class nsAtom;
class nsIContent;

namespace mozilla::dom {
class Element;
}
namespace mozilla::intl {
class Collator;
}

class nsXULContentUtils {
 protected:
  const static mozilla::intl::Collator* gCollator;

  static bool gDisableXULCache;

  static int DisableXULCacheChangedCallback(const char* aPrefName,
                                            void* aClosure);

 public:
  static nsresult Finish();

  static nsresult FindChildByTag(nsIContent* aElement, int32_t aNameSpaceID,
                                 nsAtom* aTag, mozilla::dom::Element** aResult);

  static const mozilla::intl::Collator* GetCollator();
};

#endif  // nsXULContentUtils_h__
