"use strict";

add_task(async function process_switch_in_sidebars_popups() {
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

  // Before Fission, there's no guarantee that two (independent) pages
  // from the same domain will end up in the same process.
  if (Services.appinfo.fissionAutostart) {
    is(cs1.pid, cs2.pid, "Both example.com CSs from the same process");
  }

  await closeBrowserAction(extension);
  await extension.unload();
});
