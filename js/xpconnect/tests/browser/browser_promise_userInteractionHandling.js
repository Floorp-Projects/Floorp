/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

add_task(async function test_explicit_object_prototype() {
  const url =
    "http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_promise_userInteractionHandling.html";
  await BrowserTestUtils.withNewTab(url, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      const DOMWindowUtils = EventUtils._getDOMWindowUtils(content.window);
      is(
        DOMWindowUtils.isHandlingUserInput,
        false,
        "not yet handling user input"
      );
      const button = content.document.getElementById("button");

      let resolve;
      const p = new Promise(r => {
        resolve = r;
      });

      button.addEventListener("click", () => {
        is(DOMWindowUtils.isHandlingUserInput, true, "handling user input");
        content.document.hasStorageAccess().then(() => {
          is(
            DOMWindowUtils.isHandlingUserInput,
            true,
            "still handling user input"
          );
          Promise.resolve().then(() => {
            is(
              DOMWindowUtils.isHandlingUserInput,
              false,
              "no more handling user input"
            );
            resolve();
          });
        });
      });

      EventUtils.synthesizeMouseAtCenter(button, {}, content.window);

      await p;
    });
  });
});
