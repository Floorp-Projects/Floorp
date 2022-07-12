/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Bug 1411725 - An XHR using a SubstitutingProtocolHandler channel
// (web-extension:, resource:, etc) should return the original URL,
// not the jar/file it was actually substituted for.

const TEST_URL = "resource://gre/modules/XPCOMUtils.sys.mjs";

add_task(async function test() {
  await new Promise(resolve => {
    const xhr = new XMLHttpRequest();
    xhr.responseType = "text";
    xhr.open("get", TEST_URL);
    xhr.addEventListener("loadend", () => {
      is(
        xhr.responseURL,
        TEST_URL,
        "original URL is given instead of substitution"
      );
      resolve();
    });
    xhr.send();
  });
});
