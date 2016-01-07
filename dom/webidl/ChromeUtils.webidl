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
   * A helper that converts OriginAttributesDictionary to a opaque suffix string.
   *
   * @param originAttrs       The originAttributes from the caller.
   */
  static ByteString
  originAttributesToSuffix(optional OriginAttributesDictionary originAttrs);

  /**
   * Returns true if the members of |originAttrs| match the provided members
   * of |pattern|.
   *
   * @param originAttrs       The originAttributes under consideration.
   * @param pattern           The pattern to use for matching.
   */
  static boolean
  originAttributesMatchPattern(optional OriginAttributesDictionary originAttrs,
                               optional OriginAttributesPatternDictionary pattern);

  /**
   * Returns an OriginAttributes dictionary using the origin URI but forcing
   * the passed userContextId.
   */
  [Throws]
  static OriginAttributesDictionary
  createOriginAttributesWithUserContextId(DOMString origin,
                                          unsigned long userContextId);
};

/**
 * Used by principals and the script security manager to represent origin
 * attributes. The first dictionary is designed to contain the full set of
 * OriginAttributes, the second is used for pattern-matching (i.e. does this
 * OriginAttributesDictionary match the non-empty attributes in this pattern).
 *
 * IMPORTANT: If you add any members here, you need to do the following:
 * (1) Add them to both dictionaries.
 * (2) Update the methods on mozilla::OriginAttributes, including equality,
 *     serialization, deserialization, and inheritance.
 * (3) Update the methods on mozilla::OriginAttributesPattern, including matching.
 */
dictionary OriginAttributesDictionary {
  unsigned long appId = 0;
  unsigned long userContextId = 0;
  boolean inBrowser = false;
  DOMString addonId = "";
  DOMString signedPkg = "";
};
dictionary OriginAttributesPatternDictionary {
  unsigned long appId;
  unsigned long userContextId;
  boolean inBrowser;
  DOMString addonId;
  DOMString signedPkg;
};
