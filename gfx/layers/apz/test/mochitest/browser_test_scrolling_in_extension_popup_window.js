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

  await SpecialPowers.pushPrefEnv({ set: [["apz.popups.enabled", true]] });

  // Open the popup window of the extension.
  const browserForPopup = await openBrowserActionPanel(
    extension,
    undefined,
    true
  );

  // Flush APZ repaints and waits for MozAfterPaint to make sure APZ state is
  // stable.
  await promiseApzFlushedRepaintsInPopup(browserForPopup);

  const scrollEventPromise = SpecialPowers.spawn(
    browserForPopup,
    [],
    async () => {
      return new Promise(resolve => {
        content.window.addEventListener(
          "scroll",
          event => {
            dump("Got a scroll event in the popup content document\n");
            resolve();
          },
          { once: true }
        );
      });
    }
  );

  // Send native mouse wheel to scroll the content in the popup.
  await promiseNativeWheelAndWaitForObserver(browserForPopup, 50, 50, 0, -100);

  // Flush APZ repaints and waits for MozAfterPaint to make sure the scroll has
  // been reflected on the main thread.
  const apzPromise = promiseApzFlushedRepaintsInPopup(browserForPopup);

  await Promise.all([apzPromise, scrollEventPromise]);

  const scrollY = await SpecialPowers.spawn(browserForPopup, [], () => {
    return content.window.scrollY;
  });
  ok(scrollY > 0, "Mouse wheel scrolling works in the popup window");

  await closeBrowserAction(extension);

  await extension.unload();
});
