/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtaone at http://mozilla.org/MPL/2.0/. */

/**
 * Options for nsINativeOSFileInternals::Read
 */
dictionary NativeOSFileReadOptions
{
  /**
   * If specified, convert the raw bytes to a String
   * with the specified encoding. Otherwise, return
   * the raw bytes as a TypedArray.
   */
  DOMString? encoding;

  /**
   * If specified, limit the number of bytes to read.
   */
  unsigned long long? bytes;
};
