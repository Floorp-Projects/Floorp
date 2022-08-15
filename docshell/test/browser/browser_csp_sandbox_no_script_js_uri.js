/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Test that javascript URIs in CSP-sandboxed contexts can't be used to bypass
 * script restrictions.
 */
add_task(async function test_csp_sandbox_no_script_js_uri() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "dummy_page.html",
    async browser => {
      info("Register observer and wait for javascript-uri-blocked message.");
      let observerPromise = SpecialPowers.spawn(browser, [], () => {
        return new Promise(resolve => {
          SpecialPowers.addObserver(function obs(subject) {
            ok(
              subject == content,
              "Should block script spawned via javascript uri"
            );
            SpecialPowers.removeObserver(
              obs,
              "javascript-uri-blocked-by-sandbox"
            );
            resolve();
          }, "javascript-uri-blocked-by-sandbox");
        });
      });

      info("Spawn csp-sandboxed iframe with javascript URI");
      let frameBC = await SpecialPowers.spawn(
        browser,
        [TEST_PATH + "file_csp_sandbox_no_script_js_uri.html"],
        async url => {
          let frame = content.document.createElement("iframe");
          let loadPromise = ContentTaskUtils.waitForEvent(frame, "load", true);
          frame.src = url;
          content.document.body.appendChild(frame);
          await loadPromise;
          return frame.browsingContext;
        }
      );

      info("Click javascript URI link in iframe");
      BrowserTestUtils.synthesizeMouseAtCenter("a", {}, frameBC);
      await observerPromise;
    }
  );
});
