/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

gReduceMotionOverride = true;

const AREAS = [
  "keyboard",
  "menu_bar",
  "tabs_bar",
  "nav_bar",
  "bookmarks_bar",
  "app_menu",
  "tabs_context",
  "content_context",
  "overflow_menu",
  "pinned_overflow_menu",
  "pageaction_urlbar",
  "pageaction_panel",

  "preferences_paneHome",
  "preferences_paneGeneral",
  "preferences_panePrivacy",
  "preferences_paneSearch",
  "preferences_paneSearchResults",
  "preferences_paneSync",
  "preferences_paneContainers",
];

// Checks that the correct number of clicks are registered against the correct
// keys in the scalars.
function assertInteractionScalars(expectedAreas) {
  let processScalars =
    Services.telemetry.getSnapshotForKeyedScalars("main", true)?.parent ?? {};

  for (let source of AREAS) {
    let scalars = processScalars?.[`browser.ui.interaction.${source}`] ?? {};

    let expected = expectedAreas[source] ?? {};

    let expectedKeys = new Set(
      Object.keys(scalars).concat(Object.keys(expected))
    );
    for (let key of expectedKeys) {
      Assert.equal(
        scalars[key],
        expected[key],
        `Expected to see the correct value for ${key} in ${source}.`
      );
    }
  }
}

const elem = id => document.getElementById(id);
const click = el => {
  if (typeof el == "string") {
    el = elem(el);
  }

  EventUtils.synthesizeMouseAtCenter(el, {}, window);
};

add_task(async function toolbarButtons() {
  await BrowserTestUtils.withNewTab("http://example.com", async () => {
    let customButton = await new Promise(resolve => {
      CustomizableUI.createWidget({
        // In CSS identifiers cannot start with a number but CustomizableUI accepts that.
        id: "12foo",
        onCreated: resolve,
        defaultArea: "nav-bar",
      });
    });

    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    let tabClose = BrowserTestUtils.waitForTabClosing(newTab);

    let tabs = elem("tabbrowser-tabs");
    if (!tabs.hasAttribute("overflow")) {
      tabs.setAttribute("overflow", "true");
      registerCleanupFunction(() => {
        tabs.removeAttribute("overflow");
      });
    }

    click("stop-reload-button");
    click("back-button");
    click("back-button");

    // Make sure the all tabs panel is in the document.
    gTabsPanel.initElements();
    let view = elem("allTabsMenu-allTabsView");
    let shown = BrowserTestUtils.waitForEvent(view, "ViewShown");
    click("alltabs-button");
    await shown;

    let hidden = BrowserTestUtils.waitForEvent(view, "ViewHiding");
    gTabsPanel.hideAllTabsPanel();
    await hidden;

    click(newTab.querySelector(".tab-close-button"));
    await tabClose;

    let bookmarksToolbar = gNavToolbox.querySelector("#PersonalToolbar");

    let bookmarksToolbarReady = BrowserTestUtils.waitForMutationCondition(
      bookmarksToolbar,
      { attributes: true },
      () => {
        return (
          bookmarksToolbar.getAttribute("collapsed") != "true" &&
          bookmarksToolbar.getAttribute("initialized") == "true"
        );
      }
    );

    window.setToolbarVisibility(
      bookmarksToolbar,
      true /* isVisible */,
      false /* persist */,
      false /* animated */
    );
    registerCleanupFunction(() => {
      window.setToolbarVisibility(
        bookmarksToolbar,
        false /* isVisible */,
        false /* persist */,
        false /* animated */
      );
    });
    await bookmarksToolbarReady;

    // The Bookmarks Toolbar does some optimizations to try not to jank the
    // browser when populating itself, and does so asynchronously. We wait
    // until a bookmark item is available in the DOM before continuing.
    let placesToolbarItems = document.getElementById("PlacesToolbarItems");
    await BrowserTestUtils.waitForMutationCondition(
      placesToolbarItems,
      { childList: true },
      () => placesToolbarItems.querySelector(".bookmark-item") != null
    );

    click(placesToolbarItems.querySelector(".bookmark-item"));

    click(customButton);

    assertInteractionScalars({
      nav_bar: {
        "stop-reload-button": 1,
        "back-button": 2,
        "12foo": 1,
      },
      tabs_bar: {
        "alltabs-button": 1,
        "tab-close-button": 1,
      },
      bookmarks_bar: {
        "bookmark-item": 1,
      },
    });
    CustomizableUI.destroyWidget("12foo");
  });
});

add_task(async function contextMenu() {
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    let tab = gBrowser.getTabForBrowser(browser);
    let context = elem("tabContextMenu");
    let shown = BrowserTestUtils.waitForEvent(context, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      tab,
      { type: "contextmenu", button: 2 },
      window
    );
    await shown;

    let hidden = BrowserTestUtils.waitForEvent(context, "popuphidden");
    context.activateItem(document.getElementById("context_toggleMuteTab"));
    await hidden;

    assertInteractionScalars({
      tabs_context: {
        "context-toggleMuteTab": 1,
      },
    });

    // Check that tab-related items in the toolbar menu also register telemetry:
    context = elem("toolbar-context-menu");
    shown = BrowserTestUtils.waitForEvent(context, "popupshown");
    let scrollbox = elem("tabbrowser-arrowscrollbox");
    EventUtils.synthesizeMouse(
      scrollbox,
      // offset within the scrollbox - somewhere near the end:
      scrollbox.getBoundingClientRect().width - 20,
      5,
      { type: "contextmenu", button: 2 },
      window
    );
    await shown;

    hidden = BrowserTestUtils.waitForEvent(context, "popuphidden");
    context.activateItem(
      document.getElementById("toolbar-context-selectAllTabs")
    );
    await hidden;

    assertInteractionScalars({
      tabs_context: {
        "toolbar-context-selectAllTabs": 1,
      },
    });
    // tidy up:
    gBrowser.clearMultiSelectedTabs();
  });
});

add_task(async function appMenu() {
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    let shown = BrowserTestUtils.waitForEvent(
      elem("appMenu-popup"),
      "popupshown"
    );
    click("PanelUI-menu-button");
    await shown;

    let hidden = BrowserTestUtils.waitForEvent(
      elem("appMenu-popup"),
      "popuphidden"
    );

    let findButtonID = "appMenu-find-button2";
    click(findButtonID);
    await hidden;

    let expectedScalars = {
      nav_bar: {
        "PanelUI-menu-button": 1,
      },
      app_menu: {},
    };
    expectedScalars.app_menu[findButtonID] = 1;

    assertInteractionScalars(expectedScalars);
  });
});

add_task(async function devtools() {
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    let shown = BrowserTestUtils.waitForEvent(
      elem("appMenu-popup"),
      "popupshown"
    );
    click("PanelUI-menu-button");
    await shown;

    click("appMenu-more-button2");
    shown = BrowserTestUtils.waitForEvent(
      elem("appmenu-moreTools"),
      "ViewShown"
    );
    await shown;

    let tabOpen = BrowserTestUtils.waitForNewTab(gBrowser);
    let hidden = BrowserTestUtils.waitForEvent(
      elem("appMenu-popup"),
      "popuphidden"
    );
    click(
      document.querySelector(
        "#appmenu-moreTools toolbarbutton[key='key_viewSource']"
      )
    );
    await hidden;

    let tab = await tabOpen;
    BrowserTestUtils.removeTab(tab);

    // Note that item ID's have '_' converted to '-'.
    assertInteractionScalars({
      nav_bar: {
        "PanelUI-menu-button": 1,
      },
      app_menu: {
        "appMenu-more-button2": 1,
        "key-viewSource": 1,
      },
    });
  });
});

add_task(async function webextension() {
  BrowserUsageTelemetry._resetAddonIds();

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    function background() {
      browser.commands.onCommand.addListener(() => {
        browser.test.sendMessage("oncommand");
      });

      browser.runtime.onMessage.addListener(msg => {
        if (msg == "from-sidebar-action") {
          browser.test.sendMessage("sidebar-opened");
        }
      });

      browser.test.sendMessage("ready");
    }

    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        version: "1",
        applications: {
          gecko: { id: "random_addon@example.com" },
        },
        browser_action: {
          default_icon: "default.png",
          default_title: "Hello",
        },
        page_action: {
          default_icon: "default.png",
          default_title: "Hello",
          show_matches: ["http://example.com/*"],
        },
        commands: {
          test_command: {
            suggested_key: {
              default: "Alt+Shift+J",
            },
          },
          _execute_sidebar_action: {
            suggested_key: {
              default: "Alt+Shift+Q",
            },
          },
        },
        sidebar_action: {
          default_panel: "sidebar.html",
          open_at_install: false,
        },
      },
      files: {
        "sidebar.html": `
          <!DOCTYPE html>
          <html>
            <head>
              <meta charset="utf-8">
              <script src="sidebar.js"></script>
            </head>
          </html>
        `,

        "sidebar.js": function() {
          browser.runtime.sendMessage("from-sidebar-action");
        },
      },
      background,
    });

    await extension.startup();
    await extension.awaitMessage("ready");

    // As the first add-on interacted with this should show up as `addon0`.

    click("random_addon_example_com-browser-action");
    assertInteractionScalars({
      nav_bar: {
        addon0: 1,
      },
    });

    // Wait for the element to show up.
    await TestUtils.waitForCondition(() =>
      elem("pageAction-urlbar-random_addon_example_com")
    );

    click("pageAction-urlbar-random_addon_example_com");
    assertInteractionScalars({
      pageaction_urlbar: {
        addon0: 1,
      },
    });

    EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
    await extension.awaitMessage("oncommand");
    assertInteractionScalars({
      keyboard: {
        addon0: 1,
      },
    });

    EventUtils.synthesizeKey("q", { altKey: true, shiftKey: true });
    await extension.awaitMessage("sidebar-opened");
    assertInteractionScalars({
      keyboard: {
        addon0: 1,
      },
    });

    const extension2 = ExtensionTestUtils.loadExtension({
      manifest: {
        version: "1",
        applications: {
          gecko: { id: "random_addon2@example.com" },
        },
        browser_action: {
          default_icon: "default.png",
          default_title: "Hello",
        },
        page_action: {
          default_icon: "default.png",
          default_title: "Hello",
          show_matches: ["http://example.com/*"],
        },
        commands: {
          test_command: {
            suggested_key: {
              default: "Alt+Shift+9",
            },
          },
        },
      },
      background,
    });

    await extension2.startup();
    await extension2.awaitMessage("ready");

    // A second extension should be `addon1`.

    click("random_addon2_example_com-browser-action");
    assertInteractionScalars({
      nav_bar: {
        addon1: 1,
      },
    });

    // Wait for the element to show up.
    await TestUtils.waitForCondition(() =>
      elem("pageAction-urlbar-random_addon2_example_com")
    );

    click("pageAction-urlbar-random_addon2_example_com");
    assertInteractionScalars({
      pageaction_urlbar: {
        addon1: 1,
      },
    });

    EventUtils.synthesizeKey("9", { altKey: true, shiftKey: true });
    await extension2.awaitMessage("oncommand");
    assertInteractionScalars({
      keyboard: {
        addon1: 1,
      },
    });

    // The first should have retained its ID.
    click("random_addon_example_com-browser-action");
    assertInteractionScalars({
      nav_bar: {
        addon0: 1,
      },
    });

    EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
    await extension.awaitMessage("oncommand");
    assertInteractionScalars({
      keyboard: {
        addon0: 1,
      },
    });

    click("pageAction-urlbar-random_addon_example_com");
    assertInteractionScalars({
      pageaction_urlbar: {
        addon0: 1,
      },
    });

    await extension.unload();

    // Clear the last opened ID so if this test runs again the sidebar won't
    // automatically open when the extension is installed.
    window.SidebarUI.lastOpenedId = null;

    // The second should retain its ID.
    click("random_addon2_example_com-browser-action");
    click("random_addon2_example_com-browser-action");
    assertInteractionScalars({
      nav_bar: {
        addon1: 2,
      },
    });

    click("pageAction-urlbar-random_addon2_example_com");
    assertInteractionScalars({
      pageaction_urlbar: {
        addon1: 1,
      },
    });

    EventUtils.synthesizeKey("9", { altKey: true, shiftKey: true });
    await extension2.awaitMessage("oncommand");
    assertInteractionScalars({
      keyboard: {
        addon1: 1,
      },
    });

    await extension2.unload();
  });
});

add_task(async function mainMenu() {
  // macOS does not use the menu bar.
  if (AppConstants.platform == "macosx") {
    return;
  }

  BrowserUsageTelemetry._resetAddonIds();

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    CustomizableUI.setToolbarVisibility("toolbar-menubar", true);

    let shown = BrowserTestUtils.waitForEvent(
      elem("menu_EditPopup"),
      "popupshown"
    );
    click("edit-menu");
    await shown;

    let hidden = BrowserTestUtils.waitForEvent(
      elem("menu_EditPopup"),
      "popuphidden"
    );
    click("menu_selectAll");
    await hidden;

    assertInteractionScalars({
      menu_bar: {
        // Note that the _ is replaced with - for telemetry identifiers.
        "menu-selectAll": 1,
      },
    });

    CustomizableUI.setToolbarVisibility("toolbar-menubar", false);
  });
});

add_task(async function preferences() {
  let initialized = BrowserTestUtils.waitForEvent(gBrowser, "Initialized");
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    await initialized;

    Services.telemetry.getSnapshotForKeyedScalars("main", true);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#browserRestoreSession",
      {},
      gBrowser.selectedBrowser.browsingContext
    );

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#category-search",
      {},
      gBrowser.selectedBrowser.browsingContext
    );

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#searchBarShownRadio",
      {},
      gBrowser.selectedBrowser.browsingContext
    );

    assertInteractionScalars({
      preferences_paneGeneral: {
        browserRestoreSession: 1,
      },
      preferences_paneSearch: {
        searchBarShownRadio: 1,
      },
    });
  });
});
