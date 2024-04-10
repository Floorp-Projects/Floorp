/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This script is loaded in a non-tab extension context, and starts the test by
// loading an iframe that runs contentScript as a content script.
function extensionScript() {
  let FRAME_URL = browser.runtime.getManifest().content_scripts[0].matches[0];
  // Cannot use :8888 in the manifest because of bug 1468162.
  FRAME_URL = FRAME_URL.replace("mochi.test", "mochi.test:8888");
  let FRAME_ORIGIN = new URL(FRAME_URL).origin;

  browser.runtime.onConnect.addListener(port => {
    browser.test.assertEq(port.sender.tab, undefined, "Sender is not a tab");
    browser.test.assertEq(port.sender.frameId, undefined, "frameId unset");
    browser.test.assertEq(port.sender.url, FRAME_URL, "Expected sender URL");
    browser.test.assertEq(
      port.sender.origin,
      FRAME_ORIGIN,
      "Expected sender origin"
    );

    port.onMessage.addListener(msg => {
      browser.test.assertEq("pong", msg, "Reply from content script");
      port.disconnect();
    });
    port.postMessage("ping");
  });

  browser.test.log(`Going to open ${FRAME_URL} at ${location.pathname}`);
  let f = document.createElement("iframe");
  f.src = FRAME_URL;
  document.body.appendChild(f);
}

function contentScript() {
  browser.test.log(`Running content script at ${document.URL}`);

  let port = browser.runtime.connect();
  port.onMessage.addListener(msg => {
    browser.test.assertEq("ping", msg, "Expected message to content script");
    port.postMessage("pong");
  });
  port.onDisconnect.addListener(() => {
    browser.test.sendMessage("disconnected_in_content_script");
  });
}

add_task(async function connect_from_background_frame() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/?background"],
          js: ["contentscript.js"],
          all_frames: true,
        },
      ],
    },
    files: {
      "contentscript.js": contentScript,
    },
    background: extensionScript,
  });
  await extension.startup();
  await extension.awaitMessage("disconnected_in_content_script");
  await extension.unload();
});

add_task(async function connect_from_sidebar_panel() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/?sidebar"],
          js: ["contentscript.js"],
          all_frames: true,
        },
      ],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "contentscript.js": contentScript,
      "sidebar.html": `<!DOCTYPE html><meta charset="utf-8"><body><script src="sidebar.js"></script></body>`,
      "sidebar.js": extensionScript,
    },
  });
  await extension.startup();
  await extension.awaitMessage("disconnected_in_content_script");
  await extension.unload();
});

add_task(async function connect_from_browser_action_popup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/?browser_action_popup"],
          js: ["contentscript.js"],
          all_frames: true,
        },
      ],
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
    },
    files: {
      "contentscript.js": contentScript,
      "popup.html": `<!DOCTYPE html><meta charset="utf-8"><body><script src="popup.js"></script></body>`,
      "popup.js": extensionScript,
    },
  });
  await extension.startup();
  await clickBrowserAction(extension);
  await extension.awaitMessage("disconnected_in_content_script");
  await closeBrowserAction(extension);
  await extension.unload();
});
