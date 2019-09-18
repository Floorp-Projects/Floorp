/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://encoding.spec.whatwg.org/#interface-textencoder
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

dictionary TextEncoderEncodeIntoResult {
  unsigned long long read;
  unsigned long long written;
};

[Exposed=(Window,Worker)]
interface TextEncoder {
  constructor();

  /*
   * This is DOMString in the spec, but the value is always ASCII
   * and short. By declaring this as ByteString, we get the same
   * end result (storage as inline Latin1 string in SpiderMonkey)
   * with fewer conversions.
   */
  [Constant]
  readonly attribute ByteString encoding;

  /*
   * This is spec-wise USVString but marking it as
   * JSString as an optimization. (The SpiderMonkey-provided
   * conversion to UTF-8 takes care of replacing lone
   * surrogates with the REPLACEMENT CHARACTER, so the
   * observable behavior of USVString is matched.)
   */
  [NewObject]
  Uint8Array encode(optional JSString input = "");

  /*
   * The same comment about USVString as above applies here.
   */
  [CanOOM]
  TextEncoderEncodeIntoResult encodeInto(JSString source, Uint8Array destination);
};
