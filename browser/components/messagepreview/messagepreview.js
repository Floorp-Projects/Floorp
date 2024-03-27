/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global MPShowMessage, MPIsEnabled, MPShouldShowHint */

"use strict";

// decode a 16-bit string in which only one byte of each
// 16-bit unit is occupied, to UTF-8. This is necessary to
// comply with `btoa` API constraints.
function fromBinary(encoded) {
  const binary = atob(decodeURIComponent(encoded));
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return String.fromCharCode(...new Uint16Array(bytes.buffer));
}

function decodeMessageFromUrl() {
  const url = new URL(document.location.href);

  if (url.searchParams.has("json")) {
    const encodedMessage = url.searchParams.get("json");

    return fromBinary(encodedMessage);
  }
  return null;
}

function showHint() {
  document.body.classList.add("hint-box");
  document.body.innerHTML = `<div class="hint">Message preview is not enabled. Enable it in about:config by setting <code>browser.newtabpage.activity-stream.asrouter.devtoolsEnabled</code> to true.</div>`;
}

const message = decodeMessageFromUrl();

if (message) {
  // If message preview is enabled, show the message.
  if (MPIsEnabled()) {
    MPShowMessage(message);
  } else if (MPShouldShowHint()) {
    // If running in a local build, show a hint about how to enable preview.
    if (document.body) {
      showHint();
    } else {
      document.addEventListener("DOMContentLoaded", showHint, { once: true });
    }
  }
}
