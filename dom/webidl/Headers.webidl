/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://fetch.spec.whatwg.org/#headers-class
 */

typedef (sequence<sequence<ByteString>> or record<ByteString, ByteString>) HeadersInit;

enum HeadersGuardEnum {
  "none",
  "request",
  "request-no-cors",
  "response",
  "immutable"
};

[Exposed=(Window,Worker)]
interface Headers {
  [Throws]
  constructor(optional HeadersInit init);

  [Throws] void append(ByteString name, ByteString value);
  [Throws] void delete(ByteString name);
  [Throws] ByteString? get(ByteString name);
  [Throws] boolean has(ByteString name);
  [Throws] void set(ByteString name, ByteString value);
  iterable<ByteString, ByteString>;

  // Used to test different guard states from mochitest.
  // Note: Must be set prior to populating headers or will throw.
  [ChromeOnly, SetterThrows] attribute HeadersGuardEnum guard;
};
