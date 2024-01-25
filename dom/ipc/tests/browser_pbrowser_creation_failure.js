/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_subframe_pbrowser_creation_failure() {
  await BrowserTestUtils.withNewTab(
    "https://example.com/document-builder.sjs?html=<iframe></iframe>",
    async browser => {
      let bcid = await SpecialPowers.spawn(browser, [], () => {
        return content.document.body.querySelector("iframe").browsingContext.id;
      });

      // We currently have no known way to trigger PBrowser creation failure,
      // other than to use this custom pref for the purpose.
      info(`enabling failPBrowserCreation for browsingContext: ${bcid}`);
      await SpecialPowers.pushPrefEnv({
        set: [
          ["browser.tabs.remote.testOnly.failPBrowserCreation.enabled", true],
          [
            "browser.tabs.remote.testOnly.failPBrowserCreation.browsingContext",
            `${bcid}`,
          ],
        ],
      });

      let eventFiredPromise = BrowserTestUtils.waitForEvent(
        browser,
        "oop-browser-crashed"
      );

      info("triggering navigation which will fail pbrowser creation");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.querySelector("iframe").src =
          "https://example.org/document-builder.sjs?html=frame";
      });

      info("Waiting for oop-browser-crashed event.");
      let event = await eventFiredPromise;
      ok(!event.isTopFrame, "should be reporting subframe crash");
      Assert.equal(
        event.childID,
        0,
        "childID should be zero, as no process actually crashed"
      );
      is(event.browsingContextId, bcid, "bcid should match");

      let { subject: windowGlobal } = await BrowserUtils.promiseObserved(
        "window-global-created",
        wgp => wgp.documentURI.spec.startsWith("about:framecrashed")
      );
      is(windowGlobal.browsingContext.id, bcid, "bcid is correct");

      await SpecialPowers.popPrefEnv();
    }
  );
});
