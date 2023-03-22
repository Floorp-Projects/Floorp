/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* https://w3c.github.io/webtransport/#duplex-stream */

[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.datagrams.enabled"]
interface WebTransportDatagramDuplexStream {
  readonly attribute ReadableStream readable;
  readonly attribute WritableStream writable;

  readonly attribute unsigned long maxDatagramSize;
  [Throws] attribute unrestricted double incomingMaxAge;
  [Throws] attribute unrestricted double outgoingMaxAge;
  [Throws] attribute unrestricted double incomingHighWaterMark;
  [Throws] attribute unrestricted double outgoingHighWaterMark;
};
