/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

async function test_document(url) {
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    let result = await ContentTask.spawn(
      browser, {},
      async function() {
        let result = content.document.getElementById("result");
        return result.innerText;
      }
    );
    is(result, "OK", "test succeeds");
  });
}

add_task(async function test_explicit_object_prototype() {
  await test_document("http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_realm_key_object_prototype_top.html");
});

add_task(async function test_implicit_object_prototype() {
  await test_document("http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_realm_key_promise_top.html");
});
