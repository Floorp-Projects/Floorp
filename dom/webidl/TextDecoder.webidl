/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://encoding.spec.whatwg.org/#interface-textdecoder
 */

interface mixin TextDecoderCommon {
  [Constant]
  readonly attribute DOMString encoding;
  [Constant]
  readonly attribute boolean fatal;
  [Constant]
  readonly attribute boolean ignoreBOM;
};

[Exposed=(Window,Worker)]
interface TextDecoder {
  [Throws]
  constructor(optional DOMString label = "utf-8",
              optional TextDecoderOptions options = {});

  [Throws]
  USVString decode(optional BufferSource input, optional TextDecodeOptions options = {});
};
TextDecoder includes TextDecoderCommon;

dictionary TextDecoderOptions {
  boolean fatal = false;
  boolean ignoreBOM = false;
};

dictionary TextDecodeOptions {
  boolean stream = false;
};
