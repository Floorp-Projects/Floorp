/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTTPSOnlyUtils_h___
#define nsHTTPSOnlyUtils_h___

#include "nsIScriptError.h"

class nsHTTPSOnlyUtils {
 public:
  /**
   * Determines if a request should get because of the HTTPS-Only mode
   * @param  aURI           nsIURI of request
   * @param  aLoadInfo      nsILoadInfo of request
   * @param  aShouldUpgrade true if request should get upgraded
   */
  static bool ShouldUpgradeRequest(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  /**
   * Logs localized message to either content console or browser console
   * @param aName              Localization key
   * @param aParams            Localization parameters
   * @param aFlags             Logging Flag (see nsIScriptError)
   * @param aInnerWindowID     Inner Window ID (Logged on browser console if 0)
   * @param aFromPrivateWindow If from private window
   * @param [aURI]             Optional: URI to log
   */
  static void LogLocalizedString(const char* aName,
                                 const nsTArray<nsString>& aParams,
                                 uint32_t aFlags, uint64_t aInnerWindowID,
                                 bool aFromPrivateWindow,
                                 nsIURI* aURI = nullptr);

 private:
  /**
   * Logs localized message to either content console or browser console
   * @param aMessage           Message to log
   * @param aFlags             Logging Flag (see nsIScriptError)
   * @param aInnerWindowID     Inner Window ID (Logged on browser console if 0)
   * @param aFromPrivateWindow If from private window
   * @param [aURI]             Optional: URI to log
   */
  static void LogMessage(const nsAString& aMessage, uint32_t aFlags,
                         uint64_t aInnerWindowID, bool aFromPrivateWindow,
                         nsIURI* aURI = nullptr);

  /**
   * Report simple error message to the browser console
   *   @param aErrorText the error message
   *   @param aCategory Name of the module reporting error
   *   @param aFromPrivateWindow Whether from private window or not
   *   @param aFromChromeContext Whether from chrome context or not
   *   @param [aErrorFlags] See nsIScriptError.
   */
  static void LogSimpleConsoleError(
      const nsAString& aErrorText, const char* aCategory,
      bool aFromPrivateWindow, bool aFromChromeContext,
      uint32_t aErrorFlags = nsIScriptError::errorFlag);
};

#endif /* nsHTTPSOnlyUtils_h___ */
