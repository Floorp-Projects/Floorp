/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EMPTY_URL =
  "http://example.com/browser/dom/security/test/https-first/file_empty.html";
const SUBDOCUMENT_URL =
  "https://example.com/browser/dom/security/test/https-first/file_subdocument_downgrade.sjs";

add_task(async function test_subdocument_downgrade() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We want to test HTTPS-First
      ["dom.security.https_first", true],
      // Makes it easier to detect the error
      ["security.mixed_content.block_active_content", false],
    ],
  });

  // Open a empty document with origin http://example.com, which gets upgraded
  // to https://example.com by HTTPS-First and thus is marked as
  // HTTPS_ONLY_UPGRADED_HTTPS_FIRST.
  await BrowserTestUtils.withNewTab(EMPTY_URL, async browser => {
    await SpecialPowers.spawn(
      browser,
      [SUBDOCUMENT_URL],
      async SUBDOCUMENT_URL => {
        function isCrossOriginIframe(iframe) {
          try {
            return !iframe.contentDocument;
          } catch (e) {
            return true;
          }
        }
        const subdocument = content.document.createElement("iframe");
        // We open https://example.com/.../file_subdocument_downgrade.sjs in a
        // iframe, which sends a invalid response if the scheme is https. Thus
        // we should get an error. But if we accidentally copy the
        // HTTPS_ONLY_UPGRADED_HTTPS_FIRST flag from the parent into the iframe
        // loadinfo, HTTPS-First will try to downgrade the iframe. We test that
        // this doesn't happen.
        subdocument.src = SUBDOCUMENT_URL;
        const loadPromise = new Promise(resolve => {
          subdocument.addEventListener("load", () => {
            ok(
              // If the iframe got downgraded, it should now have the origin
              // http://example.com, which we can detect as being cross-origin.
              !isCrossOriginIframe(subdocument),
              "Subdocument should not be downgraded"
            );
            resolve();
          });
        });
        content.document.body.appendChild(subdocument);
        await loadPromise;
      }
    );
  });
});
