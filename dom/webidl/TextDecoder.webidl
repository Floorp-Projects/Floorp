/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://encoding.spec.whatwg.org/#interface-textdecoder
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

[Exposed=(Window,Worker)]
interface TextDecoder {
  [Throws]
  constructor(optional DOMString label = "utf-8",
              optional TextDecoderOptions options = {});

  [Constant]
  readonly attribute DOMString encoding;
  [Constant]
  readonly attribute boolean fatal;
  [Constant]
  readonly attribute boolean ignoreBOM;
  [Throws]
  USVString decode(optional BufferSource input, optional TextDecodeOptions options = {});
};

dictionary TextDecoderOptions {
  boolean fatal = false;
  boolean ignoreBOM = false;
};

dictionary TextDecodeOptions {
  boolean stream = false;
};

