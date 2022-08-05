"use strict";

add_task(async function process_switch_in_sidebars_popups() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.content_web_accessible.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/*"],
          js: ["cs.js"],
        },
      ],

      sidebar_action: {
        default_panel: "page.html?sidebar",
      },
      browser_action: {
        default_popup: "page.html?popup",
      },
      web_accessible_resources: ["page.html"],
    },
    files: {
      "page.html": `<!DOCTYPE html><meta charset=utf-8><script src=page.js></script>`,
      async "page.js"() {
        browser.test.sendMessage("extension_page", {
          place: location.search,
          pid: await SpecialPowers.spawnChrome([], () => {
            return windowGlobalParent.osPid;
          }),
        });
        if (!location.search.endsWith("_back")) {
          window.location.href = "http://example.com/" + location.search;
        }
      },

      async "cs.js"() {
        browser.test.sendMessage("content_script", {
          url: location.href,
          pid: await this.wrappedJSObject.SpecialPowers.spawnChrome([], () => {
            return windowGlobalParent.osPid;
          }),
        });
        if (location.search === "?popup") {
          window.location.href =
            browser.runtime.getURL("page.html") + "?popup_back";
        }
      },
    },
  });

  await extension.startup();

  let sidebar = await extension.awaitMessage("extension_page");
  is(sidebar.place, "?sidebar", "Message from the extension sidebar");

  let cs1 = await extension.awaitMessage("content_script");
  is(cs1.url, "http://example.com/?sidebar", "CS on example.com in sidebar");
  isnot(sidebar.pid, cs1.pid, "Navigating to example.com changed process");

  await clickBrowserAction(extension);
  let popup = await extension.awaitMessage("extension_page");
  is(popup.place, "?popup", "Message from the extension popup");

  let cs2 = await extension.awaitMessage("content_script");
  is(cs2.url, "http://example.com/?popup", "CS on example.com in popup");
  isnot(popup.pid, cs2.pid, "Navigating to example.com changed process");

  let popup2 = await extension.awaitMessage("extension_page");
  is(popup2.place, "?popup_back", "Back at extension page in popup");
  is(popup.pid, popup2.pid, "Same process as original popup page");

  is(sidebar.pid, popup.pid, "Sidebar and popup pages from the same process");

  // There's no guarantee that two (independent) pages from the same domain will
  // end up in the same process.

  await closeBrowserAction(extension);
  await extension.unload();
});

// Test that navigating the browserAction popup between extension pages doesn't keep the
// parser blocked (See Bug 1747813).
add_task(
  async function test_navigate_browserActionPopups_shouldnot_block_parser() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        browser_action: {
          default_popup: "popup-1.html",
        },
      },
      files: {
        "popup-1.html": `<!DOCTYPE html><meta charset=utf-8><script src=popup-1.js></script><h1>Popup 1</h1>`,
        "popup-2.html": `<!DOCTYPE html><meta charset=utf-8><script src=popup-2.js></script><h1>Popup 2</h1>`,

        "popup-1.js": function() {
          browser.test.onMessage.addListener(msg => {
            if (msg !== "navigate-popup") {
              browser.test.fail(`Unexpected test message "${msg}"`);
              return;
            }
            location.href = "/popup-2.html";
          });
          window.onload = () => browser.test.sendMessage("popup-page-1");
        },

        "popup-2.js": function() {
          window.onload = () => browser.test.sendMessage("popup-page-2");
        },
      },
    });

    // Make sure the mouse isn't hovering over the browserAction widget.
    EventUtils.synthesizeMouseAtCenter(
      gURLBar.textbox,
      { type: "mouseover" },
      window
    );

    await extension.startup();

    // Triggers popup preload (otherwise we wouldn't be blocking the parser for the browserAction popup
    // and the issue wouldn't be triggered, a real user on the contrary has a pretty high chance to trigger a
    // preload while hovering the browserAction popup before opening the popup with a click).
    let widget = getBrowserActionWidget(extension).forWindow(window);
    EventUtils.synthesizeMouseAtCenter(
      widget.node,
      { type: "mouseover" },
      window
    );
    await clickBrowserAction(extension);

    await extension.awaitMessage("popup-page-1");

    extension.sendMessage("navigate-popup");

    await extension.awaitMessage("popup-page-2");
    // If the bug is triggered (e.g. it did regress), the test will get stuck waiting for
    // the test message "popup-page-2" (which will never be sent because the extension page
    // script isn't executed while the parser is blocked).
    ok(
      true,
      "Extension browserAction popup successfully navigated to popup-page-2.html"
    );

    await closeBrowserAction(extension);
    await extension.unload();
  }
);
