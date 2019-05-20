/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const InlineBase64JSON = "data:application/json;";

// opts is ignored because this is only used in local development and
// replaces a more powerful network request from Firefox that can be
// configured.
function networkRequest(url: string, opts: any): Promise<*> {
  if (url.startsWith(InlineBase64JSON)) {
    const content = atob(url.slice(url.indexOf("base64") + 7));
    return Promise.resolve({ content });
  }

  return Promise.race([
    fetch(`/get?url=${url}`).then(res => {
      if (res.status >= 200 && res.status < 300) {
        if (res.headers.get("Content-Type") === "application/wasm") {
          return res.arrayBuffer().then(buffer => ({
            content: buffer,
            isDwarf: true,
          }));
        }
        return res.text().then(text => ({ content: text }));
      }
      return Promise.reject(new Error(`failed to request ${url}`));
    }),
    new Promise((resolve, reject) => {
      setTimeout(() => reject(new Error("Connect timeout error")), 6000);
    }),
  ]);
}

module.exports = networkRequest;
