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
class nsICollation;
class nsIContent;

namespace mozilla {
namespace dom {
class Element;
}
}  // namespace mozilla

class nsXULContentUtils {
 protected:
  static nsICollation* gCollation;

  static bool gDisableXULCache;

  static int DisableXULCacheChangedCallback(const char* aPrefName,
                                            void* aClosure);

 public:
  static nsresult Finish();

  static nsresult FindChildByTag(nsIContent* aElement, int32_t aNameSpaceID,
                                 nsAtom* aTag, mozilla::dom::Element** aResult);

  static nsICollation* GetCollation();
};

#endif  // nsXULContentUtils_h__
