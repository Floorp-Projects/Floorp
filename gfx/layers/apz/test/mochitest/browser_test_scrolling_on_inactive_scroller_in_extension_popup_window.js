/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/extensions/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/extensions/test/browser/head_browserAction.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

add_task(async () => {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },

    files: {
      "popup.html": `
        <html>
          <head>
            <meta charset="utf-8">
            <style>
              * {
                padding: 0;
                margin: 0;
              }
              body {
                height: 400px;
                width: 200px;
              }
              .flex {
                width: 100vw;
                display: flex;
                flex-direction: row;
                height: 100vh;
              }
              .overflow {
                flex: 1;
                overflow: auto;
                height: 100vh;
              }
              .overflow div {
                height: 400vh;
              }
            </style>
          </head>
          <body>
            <div class="flex">
              <div class="overflow"><div>123</div></div>
              <div class="overflow"><div>123</div></div>
            </div>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["apz.popups.enabled", true],
      ["apz.wr.activate_all_scroll_frames", false],
      ["apz.wr.activate_all_scroll_frames_when_fission", false],
    ],
  });

  // Open the popup window of the extension.
  const browserForPopup = await openBrowserActionPanel(
    extension,
    undefined,
    true
  );

  // Flush APZ repaints and waits for MozAfterPaint to make sure APZ state is
  // stable.
  await promiseApzFlushedRepaintsInPopup(browserForPopup);

  // A Promise to wait for one scroll event for each scrollable element.
  const scrollEventsPromise = SpecialPowers.spawn(browserForPopup, [], () => {
    let promises = [];
    content.document.querySelectorAll(".overflow").forEach(element => {
      let promise = new Promise(resolve => {
        element.addEventListener(
          "scroll",
          () => {
            resolve();
          },
          { once: true }
        );
      });
      promises.push(promise);
    });
    return Promise.all(promises);
  });

  // Send two native mouse wheel events to scroll each scrollable element in the
  // popup.
  await promiseNativeWheelAndWaitForObserver(browserForPopup, 50, 50, 0, -100);
  await promiseNativeWheelAndWaitForObserver(browserForPopup, 150, 50, 0, -100);

  // Flush APZ repaints and waits for MozAfterPaint to make sure the scroll has
  // been reflected on the main thread.
  const apzPromise = promiseApzFlushedRepaintsInPopup(browserForPopup);

  await Promise.all([apzPromise, scrollEventsPromise]);

  const scrollTops = await SpecialPowers.spawn(browserForPopup, [], () => {
    let result = [];
    content.document.querySelectorAll(".overflow").forEach(element => {
      result.push(element.scrollTop);
    });
    return result;
  });

  ok(scrollTops[0] > 0, "Mouse wheel scrolling works in the popup window");
  ok(scrollTops[1] > 0, "Mouse wheel scrolling works in the popup window");

  await closeBrowserAction(extension);

  await extension.unload();
});
