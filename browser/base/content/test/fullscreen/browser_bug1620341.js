/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const tab1URL = `data:text/html,
  <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
      <meta charset="utf-8"/>
      <title>First tab to be loaded</title>
    </head>
    <body>
      <button>JUST A BUTTON</button>
    </body>
  </html>`;

const ORIGIN =
  "https://example.com/browser/browser/base/content/test/fullscreen/fullscreen_frame.html";

add_task(async function test_fullscreen_cross_origin() {
  async function requestFullscreenThenCloseTab() {
    await BrowserTestUtils.withNewTab(ORIGIN, async function (browser) {
      info("Start fullscreen on iframe frameAllowed");

      // Make sure there is no attribute "inDOMFullscreen" before requesting fullscreen.
      await TestUtils.waitForCondition(
        () => !document.documentElement.hasAttribute("inDOMFullscreen")
      );

      ok(
        !gBrowser.tabContainer.hasAttribute("closebuttons"),
        "Close buttons should be visible on every tab"
      );

      // Request fullscreen from iframe
      await SpecialPowers.spawn(browser, [], async function () {
        let frame = content.document.getElementById("frameAllowed");
        frame.focus();
        await SpecialPowers.spawn(frame, [], async () => {
          let frameDoc = content.document;
          const waitForFullscreen = new Promise(resolve => {
            const message = "fullscreenchange";
            function handler(evt) {
              frameDoc.removeEventListener(message, handler);
              Assert.equal(evt.type, message, `Request should be allowed`);
              resolve();
            }
            frameDoc.addEventListener(message, handler);
          });

          frameDoc.getElementById("request").click();
          await waitForFullscreen;
        });
      });

      // Make sure there is attribute "inDOMFullscreen" after requesting fullscreen.
      await TestUtils.waitForCondition(() =>
        document.documentElement.hasAttribute("inDOMFullscreen")
      );
    });
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
      ["full-screen-api.transition-duration.enter", "0 0"],
      ["full-screen-api.transition-duration.leave", "0 0"],
      ["dom.security.featurePolicy.header.enabled", true],
      ["dom.security.featurePolicy.webidl.enabled", true],
    ],
  });

  // Open a tab with tab1URL.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    tab1URL,
    true
  );

  // 1. Open another tab and load a page with two iframes.
  // 2. Request fullscreen from an iframe which is in a different origin.
  // 3. Close the tab after receiving "fullscreenchange" message.
  // Note that we don't do "doc.exitFullscreen()" before closing the tab
  // on purpose.
  await requestFullscreenThenCloseTab();

  // Wait until attribute "inDOMFullscreen" is removed.
  await TestUtils.waitForCondition(
    () => !document.documentElement.hasAttribute("inDOMFullscreen")
  );

  await TestUtils.waitForCondition(
    () => !gBrowser.tabContainer.hasAttribute("closebuttons"),
    "Close buttons should come back to every tab"
  );

  // Remove the remaining tab and leave the test.
  let tabClosed = BrowserTestUtils.waitForTabClosing(tab1);
  BrowserTestUtils.removeTab(tab1);
  await tabClosed;
});
