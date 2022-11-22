/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* https://w3c.github.io/webtransport/#web-transport-error-interface */

[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.enabled"]
interface WebTransportError : DOMException {
  constructor(optional WebTransportErrorInit init = {});

  readonly attribute WebTransportErrorSource source;
  readonly attribute octet? streamErrorCode;
};

dictionary WebTransportErrorInit {
  [Clamp] octet streamErrorCode;
  DOMString message;
};

enum WebTransportErrorSource {
  "stream",
  "session",
};
