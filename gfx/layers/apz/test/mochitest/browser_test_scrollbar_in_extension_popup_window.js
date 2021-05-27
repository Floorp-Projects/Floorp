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
              * {
                padding: 0;
                margin: 0;
              }
              body {
                height: 400px;
                width: 200px;
                overflow-y: auto;
                overflow-x: hidden;
              }
              li {
                display: flex;
                justify-content: center;
                align-items: center;
                height: 30vh;
                font-size: 200%;
              }
              li:nth-child(even){
                background-color: #ccc;
              }
            </style>
          </head>
          <body>
            <ul>
              <li>1</li>
              <li>2</li>
              <li>3</li>
              <li>4</li>
              <li>5</li>
              <li>6</li>
              <li>7</li>
              <li>8</li>
              <li>9</li>
              <li>10</li>
            </ul>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("apz.popups.enabled");
  });

  async function takeSnapshot(browserWin) {
    let browser = await openBrowserActionPanel(extension, browserWin, true);
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

  // First, take a snapshot with disabling APZ in the popup window, we assume
  // scrollbars are rendered properly there.
  await SpecialPowers.setBoolPref("apz.popups.enabled", false);
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const reference = await takeSnapshot(newWin);
  await BrowserTestUtils.closeWindow(newWin);

  // Then take a snapshot with enabling APZ.
  await SpecialPowers.setBoolPref("apz.popups.enabled", true);
  const anotherWin = await BrowserTestUtils.openNewBrowserWindow();
  const test = await takeSnapshot(anotherWin);
  await BrowserTestUtils.closeWindow(anotherWin);

  is(
    test,
    reference,
    "Contents in popup window opened by extension should be same regardless of the APZ state in the window"
  );

  await extension.unload();
});
