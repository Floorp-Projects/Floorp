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

    let bookmarksToolbarVisible = TestUtils.waitForCondition(() => {
      let toolbar = gNavToolbox.querySelector("#PersonalToolbar");
      return toolbar.getAttribute("collapsed") != "true";
    }, "waiting for toolbar to become visible");
    CustomizableUI.setToolbarVisibility("PersonalToolbar", true);
    registerCleanupFunction(() => {
      CustomizableUI.setToolbarVisibility("PersonalToolbar", false);
    });
    await bookmarksToolbarVisible;

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
    click(document.querySelector("#PlacesToolbarItems .bookmark-item"));

    // Page action panel is removed in proton
    if (gProton) {
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
    } else {
      click("pageActionButton");
      let pagePanel = elem("pageActionPanel");
      shown = BrowserTestUtils.waitForEvent(pagePanel, "popupshown");
      await shown;

      hidden = BrowserTestUtils.waitForEvent(pagePanel, "popuphidden");
      click("pageAction-panel-copyURL");
      await hidden;

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
        pageaction_urlbar: {
          pageActionButton: 1,
        },
        pageaction_panel: {
          copyURL: 1,
        },
      });
    }

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
    click("context_toggleMuteTab");
    await hidden;

    assertInteractionScalars({
      tabs_context: {
        "context-toggleMuteTab": 1,
      },
    });
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

    let findButtonID = PanelUI.protonAppMenuEnabled
      ? "appMenu-find-button2"
      : "appMenu-find-button";
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

    // With the Proton AppMenu enabled, the View Source item is under
    // the "More Tools" menu, along with the rest of the Developer Tools.
    if (PanelUI.protonAppMenuEnabled) {
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
    } else {
      click("appMenu-developer-button");
      shown = BrowserTestUtils.waitForEvent(
        elem("PanelUI-developer"),
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
          "#PanelUI-developer toolbarbutton[key='key_viewSource']"
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
          "appMenu-developer-button": 1,
          "key-viewSource": 1,
        },
      });
    }
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
