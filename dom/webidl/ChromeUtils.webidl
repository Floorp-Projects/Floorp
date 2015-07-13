/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A collection of static utility methods that are only exposed to Chrome. This
 * interface is not exposed in workers, while ThreadSafeChromeUtils is.
 */
[ChromeOnly, Exposed=(Window,System)]
interface ChromeUtils : ThreadSafeChromeUtils {
  /**
   * A helper that converts OriginAttributesDictionary to cookie jar opaque
   * identfier.
   *
   * @param originAttrs       The originAttributes from the caller.
   */
  static ByteString
  originAttributesToCookieJar(optional OriginAttributesDictionary originAttrs);

  /**
   * A helper that converts OriginAttributesDictionary to a opaque suffix string.
   *
   * @param originAttrs       The originAttributes from the caller.
   */
  static ByteString
  originAttributesToSuffix(optional OriginAttributesDictionary originAttrs);
};

/**
 * Used by principals and the script security manager to represent origin
 * attributes.
 *
 * IMPORTANT: If you add any members here, you need to update the
 * methods on mozilla::OriginAttributes, and bump the CIDs of all
 * the principal implementations that use OriginAttributes in their
 * nsISerializable implementations.
 */
dictionary OriginAttributesDictionary {
  unsigned long appId = 0;
  boolean inBrowser = false;
  DOMString addonId = "";
};
