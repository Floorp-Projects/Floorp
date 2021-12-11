/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* internal interface for observing CSS style sheet loads */

#ifndef nsICSSLoaderObserver_h___
#define nsICSSLoaderObserver_h___

#include "nsISupports.h"

#define NS_ICSSLOADEROBSERVER_IID                    \
  {                                                  \
    0xf51fbf2c, 0xfe4b, 0x4a15, {                    \
      0xaf, 0x7e, 0x5e, 0x20, 0x64, 0x5f, 0xaf, 0x58 \
    }                                                \
  }

namespace mozilla {
class StyleSheet;
}

class nsICSSLoaderObserver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSLOADEROBSERVER_IID)

  /**
   * StyleSheetLoaded is called after aSheet is marked complete and before any
   * load events associated with aSheet are fired.
   * @param aSheet the sheet that was loaded. Guaranteed to always be
   *        non-null, even if aStatus indicates failure.
   * @param aWasDeferred whether the sheet load was deferred, due to it being an
   *        alternate sheet, or having a non-matching media list.
   * @param aStatus is a success code if the sheet loaded successfully and a
   *        failure code otherwise.  Note that successful load of aSheet
   *        doesn't indicate anything about whether the data actually parsed
   *        as CSS, and doesn't indicate anything about the status of any child
   *        sheets of aSheet.
   */
  NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSLoaderObserver, NS_ICSSLOADEROBSERVER_IID)

#endif  // nsICSSLoaderObserver_h___
