/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

async function networkRequest(url, opts) {
  const supportedProtocols = ["http:", "https:", "data:"];

  // Add file, chrome or moz-extension if the initial source was served by the
  // same protocol.
  const ADDITIONAL_PROTOCOLS = ["chrome:", "file:", "moz-extension:"];
  for (const protocol of ADDITIONAL_PROTOCOLS) {
    if (opts.sourceMapBaseURL?.startsWith(protocol)) {
      supportedProtocols.push(protocol);
    }
  }

  if (supportedProtocols.every(protocol => !url.startsWith(protocol))) {
    throw new Error(`unsupported protocol for sourcemap request ${url}`);
  }

  const response = await fetch(url, {
    cache: opts.loadFromCache ? "default" : "no-cache",
    // See Bug 1899389, by default fetch calls from the system principal no
    // longer use credentials.
    credentials: "same-origin",
    redirect: opts.allowRedirects ? "follow" : "error",
  });

  if (response.ok) {
    if (response.headers.get("Content-Type") === "application/wasm") {
      const buffer = await response.arrayBuffer();
      return {
        content: buffer,
        isDwarf: true,
      };
    }
    const text = await response.text();
    return { content: text };
  }

  throw new Error(`request failed with status ${response.status}`);
}

module.exports = { networkRequest };
