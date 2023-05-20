/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN =
  "https://example.com/browser/browser/base/content/test/fullscreen/fullscreen_frame.html";

add_task(async function test_fullscreen_cross_origin() {
  async function requestFullscreen(aAllow, aExpect) {
    await BrowserTestUtils.withNewTab(ORIGIN, async function (browser) {
      const iframeId = aExpect == "allowed" ? "frameAllowed" : "frameDenied";

      info("Start fullscreen on iframe " + iframeId);
      await SpecialPowers.spawn(
        browser,
        [{ aExpect, iframeId }],
        async function (args) {
          let frame = content.document.getElementById(args.iframeId);
          frame.focus();
          await SpecialPowers.spawn(frame, [args.aExpect], async expect => {
            let frameDoc = content.document;
            const waitForFullscreen = new Promise(resolve => {
              const message =
                expect == "allowed" ? "fullscreenchange" : "fullscreenerror";
              function handler(evt) {
                frameDoc.removeEventListener(message, handler);
                Assert.equal(evt.type, message, `Request should be ${expect}`);
                frameDoc.exitFullscreen();
                resolve();
              }
              frameDoc.addEventListener(message, handler);
            });
            frameDoc.getElementById("request").click();
            await waitForFullscreen;
          });
        }
      );

      if (aExpect == "allowed") {
        waitForFullScreenState(browser, false);
      }
    });
  }

  await new Promise(r => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["full-screen-api.enabled", true],
          ["full-screen-api.allow-trusted-requests-only", false],
          ["full-screen-api.transition-duration.enter", "0 0"],
          ["full-screen-api.transition-duration.leave", "0 0"],
          ["dom.security.featurePolicy.header.enabled", true],
          ["dom.security.featurePolicy.webidl.enabled", true],
        ],
      },
      r
    );
  });

  await requestFullscreen(undefined, "denied");
  await requestFullscreen("fullscreen", "allowed");
});
