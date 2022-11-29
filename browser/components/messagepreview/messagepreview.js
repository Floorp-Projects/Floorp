/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global MPShowMessage */

"use strict";

function decodeMessageFromUrl() {
  const url = new URL(document.location.href);

  if (url.searchParams.has("json")) {
    const encodedMessage = url.searchParams.get("json");

    return atob(encodedMessage);
  }
  return null;
}

const message = decodeMessageFromUrl();

if (message) {
  MPShowMessage(message);
}
