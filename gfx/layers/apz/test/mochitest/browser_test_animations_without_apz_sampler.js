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
              #target {
                width: 100px;
                height: 50px;
                background: green;
              }
              #target2 {
                width: 100px;
                height: 50px;
                background: green;
              }
            </style>
          </head>
          <body>
            <div id="target"></div>
            <div id="target2"></div>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  async function takeSnapshot(browserWin, callback) {
    let browser = await openBrowserActionPanel(extension, browserWin, true);

    if (callback) {
      await SpecialPowers.spawn(browser, [], callback);
    }

    // Ensure there's no pending paint requests.
    // The below code is a simplified version of promiseAllPaintsDone in
    // paint_listener.js.
    await SpecialPowers.spawn(browser, [], async () => {
      return new Promise(resolve => {
        function waitForPaints() {
          // Wait until paint suppression has ended
          if (SpecialPowers.DOMWindowUtils.paintingSuppressed) {
            dump`waiting for paint suppression to end...`;
            content.window.setTimeout(waitForPaints, 0);
            return;
          }

          if (SpecialPowers.DOMWindowUtils.isMozAfterPaintPending) {
            dump`waiting for paint...`;
            content.window.addEventListener("MozAfterPaint", waitForPaints, {
              once: true,
            });
            return;
          }
          resolve();
        }
        waitForPaints();
      });
    });

    const snapshot = await SpecialPowers.spawn(browser, [], async () => {
      return SpecialPowers.snapshotWindowWithOptions(
        content.window,
        undefined /* use the default rect */,
        undefined /* use the default bgcolor */,
        { DRAWWINDOW_DRAW_VIEW: true } /* to capture scrollbars */
      )
        .toDataURL()
        .toString();
    });

    const popup = getBrowserActionPopup(extension, browserWin);
    await closeBrowserAction(extension, browserWin);
    is(popup.state, "closed", "browserAction popup has been closed");

    return snapshot;
  }

  // Test without apz sampler.
  await SpecialPowers.pushPrefEnv({ set: [["apz.popups.enabled", false]] });

  // Reference
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const reference = await takeSnapshot(newWin);
  await BrowserTestUtils.closeWindow(newWin);

  // Test target
  const testWin = await BrowserTestUtils.openNewBrowserWindow();
  const result = await takeSnapshot(testWin, async () => {
    let div = content.window.document.getElementById("target");
    const anim = div.animate({ opacity: [1, 0.5] }, 10);
    await anim.finished;
    const anim2 = div.animate(
      { transform: ["translateX(10px)", "translateX(20px)"] },
      10
    );
    await anim2.finished;

    let div2 = content.window.document.getElementById("target2");
    const anim3 = div2.animate(
      { transform: ["translateX(10px)", "translateX(20px)"] },
      10
    );
    await anim3.finished;
  });
  await BrowserTestUtils.closeWindow(testWin);

  is(result, reference, "The omta property value should be reset");

  await extension.unload();
});
