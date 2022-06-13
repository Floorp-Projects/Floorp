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
            <script src="popup.js"></script>
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
      "popup.js": function() {
        window.addEventListener(
          "mousemove",
          () => {
            dump("Got a mousemove event in the popup content document\n");
            browser.test.sendMessage("received-mousemove");
          },
          { once: true }
        );
        window.addEventListener(
          "scroll",
          () => {
            dump("Got a scroll event in the popup content document\n");
            browser.test.sendMessage("received-scroll");
          },
          { once: true }
        );
      },
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

  if (!browserForPopup.isRemoteBrowser) {
    await closeBrowserAction(extension);
    await extension.unload();
    ok(
      true,
      "Skipping this test since the popup window doesn't have remote contents"
    );
    return;
  }

  // Flush APZ repaints and waits for MozAfterPaint to make sure APZ state is
  // stable.
  await promiseApzFlushedRepaintsInPopup(browserForPopup);

  const { screenX, screenY, viewId, presShellId } = await SpecialPowers.spawn(
    browserForPopup,
    [],
    () => {
      const winUtils = SpecialPowers.getDOMWindowUtils(content.window);
      return {
        screenX: content.window.mozInnerScreenX * content.devicePixelRatio,
        screenY: content.window.mozInnerScreenY * content.devicePixelRatio,
        viewId: winUtils.getViewId(content.document.documentElement),
        presShellId: winUtils.getPresShellId(),
      };
    }
  );

  // Before starting autoscroll we need to make sure a mousemove event has been
  // processed in the popup content so that subsequent mousemoves for autoscroll
  // will be properly processed in autoscroll animation.
  const mousemoveEventPromise = extension.awaitMessage("received-mousemove");

  const nativeMouseEventPromise = promiseNativeMouseEventWithAPZ({
    type: "mousemove",
    target: browserForPopup,
    offsetX: 100,
    offsetY: 50,
  });

  await Promise.all([nativeMouseEventPromise, mousemoveEventPromise]);

  const scrollEventPromise = extension.awaitMessage("received-scroll");

  // Start autoscrolling.
  ok(
    browserForPopup.browsingContext.startApzAutoscroll(
      screenX + 100,
      screenY + 50,
      viewId,
      presShellId
    )
  );

  // Send sequential mousemove events to cause autoscrolling.
  for (let i = 0; i < 10; i++) {
    await promiseNativeMouseEventWithAPZ({
      type: "mousemove",
      target: browserForPopup,
      offsetX: 100,
      offsetY: 50 + i * 10,
    });
  }

  // Flush APZ repaints and waits for MozAfterPaint to make sure the scroll has
  // been reflected on the main thread.
  const apzPromise = promiseApzFlushedRepaintsInPopup(browserForPopup);

  await Promise.all([apzPromise, scrollEventPromise]);

  const scrollY = await SpecialPowers.spawn(browserForPopup, [], () => {
    return content.window.scrollY;
  });
  ok(scrollY > 0, "Autoscrolling works in the popup window");

  browserForPopup.browsingContext.stopApzAutoscroll(viewId, presShellId);

  await closeBrowserAction(extension);

  await extension.unload();
});
