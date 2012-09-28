/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://wiki.whatwg.org/wiki/StringEncoding
 *
 * Copyright © 2006 The WHATWG Contributors
 * http://wiki.whatwg.org/wiki/WHATWG_Wiki:Copyrights
 */

[Constructor(optional DOMString encoding)]
interface TextEncoder {
  [SetterThrows]
  readonly attribute DOMString encoding;
  [Throws]
  Uint8Array encode(DOMString? string, optional TextEncodeOptions options);
};

dictionary TextEncodeOptions {
  boolean stream = false;
};

