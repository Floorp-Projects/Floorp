/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Extensions can be loaded in 3 ways: as a sidebar, as a browser action,
// or as a page action. We use these constants to alter the extension
// manifest, content script, background script, and setup conventions.
const TESTS = {
  SIDEBAR: "sidebar",
  BROWSER_ACTION: "browserAction",
  PAGE_ACTION: "pageAction",
};

function promiseBrowserReflow(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    return new Promise(resolve => {
      content.window.requestAnimationFrame(() => {
        content.window.requestAnimationFrame(resolve);
      });
    });
  });
}

async function promiseBrowserZoom(browser, extension) {
  await promiseBrowserReflow(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "html",
    { type: "mousedown", button: 0 },
    browser
  );
  return extension.awaitMessage("zoom");
}

async function test_mousewheel_zoom(test) {
  info(`Starting test of ${test} extension.`);
  let browser;

  // Scroll on Ctrl + mousewheel
  SpecialPowers.pushPrefEnv({ set: [["mousewheel.with_control.action", 3]] });

  function contentScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    document.addEventListener("mousedown", e => {
      // Send the zoom level back as a "zoom" message.
      const zoom = SpecialPowers.getFullZoom(window).toFixed(2);
      browser.test.sendMessage("zoom", zoom);
    });
  }

  function sidebarContentScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    document.addEventListener("mousedown", e => {
      // Send the zoom level back as a "zoom" message.
      const zoom = SpecialPowers.getFullZoom(window).toFixed(2);
      browser.test.sendMessage("zoom", zoom);
    });
    browser.test.sendMessage("content-loaded");
  }

  function pageActionBackgroundScript() {
    browser.tabs.query({ active: true, currentWindow: true }, tabs => {
      const tabId = tabs[0].id;

      browser.pageAction.show(tabId).then(() => {
        browser.test.sendMessage("content-loaded");
      });
    });
  }

  let manifest;
  if (test == TESTS.SIDEBAR) {
    manifest = {
      sidebar_action: {
        default_panel: "panel.html",
      },
    };
  } else if (test == TESTS.BROWSER_ACTION) {
    manifest = {
      browser_action: {
        default_popup: "panel.html",
        default_area: "navbar",
      },
    };
  } else if (test == TESTS.PAGE_ACTION) {
    manifest = {
      page_action: {
        default_popup: "panel.html",
      },
    };
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
    files: {
      "panel.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="panel.js"></script>
          </head>
          <body>
            <h1>Please Zoom Me</h1>
          </body>
        </html>
      `,
      "panel.js": test == TESTS.SIDEBAR ? sidebarContentScript : contentScript,
    },
    background:
      test == TESTS.PAGE_ACTION ? pageActionBackgroundScript : undefined,
  });

  await extension.startup();
  info("Awaiting notification that extension has loaded.");

  if (test == TESTS.SIDEBAR) {
    await extension.awaitMessage("content-loaded");

    const sidebar = document.getElementById("sidebar-box");
    ok(!sidebar.hidden, "Sidebar box is visible");

    browser = SidebarUI.browser.contentWindow.gBrowser.selectedBrowser;
  } else if (test == TESTS.BROWSER_ACTION) {
    browser = await openBrowserActionPanel(extension, undefined, true);
  } else if (test == TESTS.PAGE_ACTION) {
    await extension.awaitMessage("content-loaded");

    clickPageAction(extension, window);

    browser = await awaitExtensionPanel(extension);
  }

  info(`Requesting initial zoom from ${test} extension.`);
  let initialZoom = await promiseBrowserZoom(browser, extension);
  info(`Extension (${test}) initial zoom is ${initialZoom}.`);

  // Attempt to change the zoom of the extension with a mousewheel event.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "html",
    {
      wheel: true,
      ctrlKey: true,
      deltaY: -1,
      deltaMode: WheelEvent.DOM_DELTA_LINE,
    },
    browser
  );

  info(`Requesting changed zoom from ${test} extension.`);
  let changedZoom = await promiseBrowserZoom(browser, extension);
  info(`Extension (${test}) changed zoom is ${changedZoom}.`);
  isnot(
    changedZoom,
    initialZoom,
    `Extension (${test}) zoom was changed as expected.`
  );

  // Attempt to restore the zoom of the extension with a mousewheel event.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "html",
    {
      wheel: true,
      ctrlKey: true,
      deltaY: 1,
      deltaMode: WheelEvent.DOM_DELTA_LINE,
    },
    browser
  );

  info(`Requesting changed zoom from ${test} extension.`);
  let finalZoom = await promiseBrowserZoom(browser, extension);
  is(
    finalZoom,
    initialZoom,
    `Extension (${test}) zoom was restored as expected.`
  );

  await extension.unload();
}

// Actually trigger the tests. Bind test_mousewheel_zoom each time so we
// capture the test type.
for (const t in TESTS) {
  add_task(test_mousewheel_zoom.bind(this, TESTS[t]));
}
