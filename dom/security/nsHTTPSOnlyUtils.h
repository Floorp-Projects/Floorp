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
   * Determines if a request should get upgraded because of the HTTPS-Only mode.
   * If true, the httpsOnlyStatus flag in LoadInfo gets updated and a message is
   * logged in the console.
   * @param  aURI      nsIURI of request
   * @param  aLoadInfo nsILoadInfo of request
   * @return           true if request should get upgraded
   */
  static bool ShouldUpgradeRequest(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  /**
   * Determines if a request should get upgraded because of the HTTPS-Only mode.
   * If true, a message is logged in the console.
   * @param  aURI               nsIURI of request
   * @param  innerWindowId      Inner Window ID
   * @param  aFromPrivateWindow Whether this request comes from a private window
   * @param  httpsOnlyStatus    httpsOnlyStatus from nsILoadInfo
   * @return                    true if request should get upgraded
   */
  static bool ShouldUpgradeWebSocket(nsIURI* aURI, int32_t aInnerWindowId,
                                     bool aFromPrivateWindow,
                                     uint32_t aHttpsOnlyStatus);

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
   * Checks whether the URI ends with .onion
   * @param  aURI URI object
   * @return      true if the URI is an Onion URI
   */
  static bool OnionException(nsIURI* aURI);

  /**
   * Checks whether the URI is a loopback- or local-IP
   * @param  aURI URI object
   * @return      true if the URI is either loopback or local
   */
  static bool LoopbackOrLocalException(nsIURI* aURI);
};
#endif /* nsHTTPSOnlyUtils_h___ */
