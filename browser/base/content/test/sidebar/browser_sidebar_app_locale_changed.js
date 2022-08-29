/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that the sidebar recreates the contents of the <tree> element
 * for live app locale switching.
 */

add_task(function cleanup() {
  registerCleanupFunction(() => {
    SidebarUI.hide();
  });
});

/**
 * @param {string} sidebarName
 */
async function testLiveReloading(sidebarName) {
  info("Showing the sidebar " + sidebarName);
  await SidebarUI.show(sidebarName);

  function getTreeChildren() {
    const sidebarDoc = document.querySelector("#sidebar").contentWindow
      .document;
    return sidebarDoc.querySelector(".sidebar-placesTreechildren");
  }

  const childrenBefore = getTreeChildren();
  ok(childrenBefore, "Found the sidebar children");
  is(childrenBefore, getTreeChildren(), "The children start out as equal");

  info("Simulating an app locale change.");
  Services.obs.notifyObservers(null, "intl:app-locales-changed");

  await TestUtils.waitForCondition(
    getTreeChildren,
    "Waiting for a new child tree element."
  );

  isnot(
    childrenBefore,
    getTreeChildren(),
    "The tree's contents are re-computed."
  );

  info("Hiding the sidebar");
  SidebarUI.hide();
}

add_task(async function test_bookmarks_sidebar() {
  await testLiveReloading("viewBookmarksSidebar");
});

add_task(async function test_history_sidebar() {
  await testLiveReloading("viewHistorySidebar");
});

add_task(async function test_ext_sidebar_panel_reloaded_on_locale_changes() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    useAddonManager: "temporary",

    files: {
      "sidebar.html": `<html>
          <head>
            <meta charset="utf-8"/>
            <script src="sidebar.js"></script>
          </head>
          <body>
            A Test Sidebar
          </body>
        </html>`,
      "sidebar.js": function() {
        const { browser } = this;
        window.onload = () => {
          browser.test.sendMessage("sidebar");
        };
      },
    },
  });
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");

  // Test sidebar is opened on simulated locale changes.
  info("Switch browser to bidi and expect the sidebar panel to be reloaded");

  await SpecialPowers.pushPrefEnv({
    set: [["intl.l10n.pseudo", "bidi"]],
  });
  await extension.awaitMessage("sidebar");
  is(
    window.document.documentElement.getAttribute("dir"),
    "rtl",
    "browser window changed direction to rtl as expected"
  );

  await SpecialPowers.popPrefEnv();
  await extension.awaitMessage("sidebar");
  is(
    window.document.documentElement.getAttribute("dir"),
    "ltr",
    "browser window changed direction to ltr as expected"
  );

  await extension.unload();
});
