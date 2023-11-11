/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://encoding.spec.whatwg.org/#interface-textencoder
 */

interface mixin TextEncoderCommon {
  /*
   * This is DOMString in the spec, but the value is always ASCII
   * and short. By declaring this as ByteString, we get the same
   * end result (storage as inline Latin1 string in SpiderMonkey)
   * with fewer conversions.
   */
  [Constant]
  readonly attribute ByteString encoding;
};

dictionary TextEncoderEncodeIntoResult {
  unsigned long long read;
  unsigned long long written;
};

[Exposed=(Window,Worker)]
interface TextEncoder {
  constructor();

  /*
   * This is spec-wise USVString but marking it as UTF8String as an
   * optimization. (The SpiderMonkey-provided conversion to UTF-8 takes care of
   * replacing lone surrogates with the REPLACEMENT CHARACTER, so the
   * observable behavior of USVString is matched.)
   */
  [NewObject, Throws]
  Uint8Array encode(optional UTF8String input = "");

  /*
   * The same comment about UTF8String as above applies here with JSString.
   *
   * We use JSString because we don't want to encode the full string, just as
   * much as the capacity of the Uint8Array.
   */
  [CanOOM]
  TextEncoderEncodeIntoResult encodeInto(JSString source, Uint8Array destination);
};
TextEncoder includes TextEncoderCommon;
