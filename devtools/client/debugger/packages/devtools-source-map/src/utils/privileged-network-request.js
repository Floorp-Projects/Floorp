/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function networkRequest(url, opts) {
  const supportedProtocols = ["http:", "https:", "data:"];

  // Add file, chrome or moz-extension iff the initial source was served by the
  // same protocol.
  const ADDITIONAL_PROTOCOLS = ["chrome:", "file:", "moz-extension:"];
  for (const protocol of ADDITIONAL_PROTOCOLS) {
    if (opts.sourceMapBaseURL?.startsWith(protocol)) {
      supportedProtocols.push(protocol);
    }
  }

  if (supportedProtocols.every(protocol => !url.startsWith(protocol))) {
    return Promise.reject(`unsupported protocol for sourcemap request ${url}`);
  }

  return fetch(url, {
    cache: opts.loadFromCache ? "default" : "no-cache",
  }).then(res => {
    if (res.status >= 200 && res.status < 300) {
      if (res.headers.get("Content-Type") === "application/wasm") {
        return res.arrayBuffer().then(buffer => ({
          content: buffer,
          isDwarf: true,
        }));
      }
      return res.text().then(text => ({ content: text }));
    }
    return Promise.reject(`request failed with status ${res.status}`);
  });
}

module.exports = { networkRequest };
