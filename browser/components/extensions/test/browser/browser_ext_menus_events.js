/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";
const PAGE_BASE = PAGE.replace("context.html", "");
const PAGE_HOST_PATTERN = "http://mochi.test/*";

const EXPECT_TARGET_ELEMENT = 13337;

async function grantOptionalPermission(extension, permissions) {
  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  return ExtensionPermissions.add(extension.id, permissions, ext);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

// Registers a context menu using menus.create(menuCreateParams) and checks
// whether the menus.onShown and menus.onHidden events are fired as expected.
// doOpenMenu must open the menu and its returned promise must resolve after the
// menu is shown. Similarly, doCloseMenu must hide the menu.
async function testShowHideEvent({
  menuCreateParams,
  doOpenMenu,
  doCloseMenu,
  expectedShownEvent,
  expectedShownEventWithPermissions = null,
  forceTabToBackground = false,
  manifest_version = 2,
}) {
  async function background() {
    function awaitMessage(expectedId) {
      return new Promise(resolve => {
        browser.test.log(`Waiting for message: ${expectedId}`);
        browser.test.onMessage.addListener(function listener(id, msg) {
          // Temporary work-around for https://bugzil.la/1428213
          // TODO Bug 1428213: remove workaround for onMessage.removeListener
          if (listener._wasCalled) {
            return;
          }
          listener._wasCalled = true;
          browser.test.assertEq(expectedId, id, "Expected message");
          browser.test.onMessage.removeListener(listener);
          resolve(msg);
        });
      });
    }

    let menuCreateParams = await awaitMessage("create-params");
    const [tab] = await browser.tabs.query({
      active: true,
      currentWindow: true,
    });

    let shownEvents = [];
    let hiddenEvents = [];

    browser.menus.onShown.addListener((...args) => {
      if (args[0].targetElementId) {
        // In this test, we aren't interested in the exact value,
        // only in whether it is set or not.
        args[0].targetElementId = 13337; // = EXPECT_TARGET_ELEMENT
      }
      shownEvents.push(args[0]);
      if (menuCreateParams.title.includes("TEST_EXPECT_NO_TAB")) {
        browser.test.assertEq(undefined, args[1], "expect no tab");
      } else {
        browser.test.assertEq(tab.id, args[1].id, "expected tab");
      }
      browser.test.assertEq(2, args.length, "expected number of onShown args");
    });
    browser.menus.onHidden.addListener(event => hiddenEvents.push(event));

    await browser.pageAction.show(tab.id);

    let menuId;
    await new Promise(resolve => {
      menuId = browser.menus.create(menuCreateParams, resolve);
    });
    browser.test.assertEq(0, shownEvents.length, "no onShown before menu");
    browser.test.assertEq(0, hiddenEvents.length, "no onHidden before menu");
    browser.test.sendMessage("menu-registered", menuId);

    await awaitMessage("assert-menu-shown");
    browser.test.assertEq(1, shownEvents.length, "expected onShown");
    browser.test.assertEq(0, hiddenEvents.length, "no onHidden before closing");
    browser.test.sendMessage("onShown-event-data", shownEvents[0]);

    await awaitMessage("assert-menu-hidden");
    browser.test.assertEq(1, shownEvents.length, "expected no more onShown");
    browser.test.assertEq(1, hiddenEvents.length, "expected onHidden");
    browser.test.sendMessage("onHidden-event-data", hiddenEvents[0]);

    await awaitMessage("optional-menu-shown-with-permissions");
    browser.test.assertEq(2, shownEvents.length, "expected second onShown");
    browser.test.sendMessage("onShown-event-data2", shownEvents[1]);
  }

  const someOtherTab = gBrowser.selectedTab;
  // Tab must initially open as a foreground tab, because the test extension
  // looks for the active tab.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  const action = manifest_version < 3 ? "browser_action" : "action";
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version,
      page_action: {},
      [action]: {
        default_popup: "popup.html",
      },
      permissions: ["menus"],
      optional_permissions: [PAGE_HOST_PATTERN],
    },
    files: {
      "popup.html": `<!DOCTYPE html><meta charset="utf-8">Popup body`,
    },
  });
  await extension.startup();
  extension.sendMessage("create-params", menuCreateParams);
  let menuId = await extension.awaitMessage("menu-registered");

  if (forceTabToBackground) {
    gBrowser.selectedTab = someOtherTab;
  }

  await doOpenMenu(extension, tab);
  extension.sendMessage("assert-menu-shown");
  let shownEvent = await extension.awaitMessage("onShown-event-data");

  // menuCreateParams.id is not set, therefore a numeric ID is generated.
  expectedShownEvent.menuIds = [menuId];
  Assert.deepEqual(shownEvent, expectedShownEvent, "expected onShown info");

  await doCloseMenu(extension);
  extension.sendMessage("assert-menu-hidden");
  let hiddenEvent = await extension.awaitMessage("onHidden-event-data");
  is(hiddenEvent, undefined, "expected no event data for onHidden event");

  if (expectedShownEventWithPermissions) {
    expectedShownEventWithPermissions.menuIds = [menuId];
    await grantOptionalPermission(extension, {
      permissions: [],
      origins: [PAGE_HOST_PATTERN],
    });
    await doOpenMenu(extension, tab);
    extension.sendMessage("optional-menu-shown-with-permissions");
    let shownEvent2 = await extension.awaitMessage("onShown-event-data2");
    Assert.deepEqual(
      shownEvent2,
      expectedShownEventWithPermissions,
      "expected onShown info when host permissions are enabled"
    );
    await doCloseMenu(extension);
  }

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
}

// Make sure that we won't trigger onShown when extensions cannot add menus.
add_task(async function test_no_show_hide_for_unsupported_menu() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let events = [];
      browser.menus.onShown.addListener(data => events.push(data));
      browser.menus.onHidden.addListener(() => events.push("onHidden"));
      browser.test.onMessage.addListener(() => {
        browser.test.assertEq(
          "[]",
          JSON.stringify(events),
          "Should not have any events when the context is unsupported."
        );
        browser.test.notifyPass("done listening to menu events");
      });
    },
    manifest: {
      permissions: ["menus"],
    },
  });

  await extension.startup();
  // Open and close a menu for which the extension cannot add menu items.
  await openChromeContextMenu("toolbar-context-menu", "#stop-reload-button");
  await closeChromeContextMenu("toolbar-context-menu");

  extension.sendMessage("check menu events");
  await extension.awaitFinish("done listening to menu events");

  await extension.unload();
});

add_task(async function test_show_hide_without_menu_item() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let events = [];
      browser.menus.onShown.addListener(data => events.push(data));
      browser.menus.onHidden.addListener(() => events.push("onHidden"));
      browser.test.onMessage.addListener(() => {
        browser.test.sendMessage("events from menuless extension", events);
      });

      browser.menus.create({
        title: "never shown",
        documentUrlPatterns: ["*://url-pattern-that-never-matches/*"],
        contexts: ["all"],
      });
    },
    manifest: {
      permissions: ["menus", PAGE_HOST_PATTERN],
    },
  });

  await extension.startup();

  // Run another context menu test where onShown/onHidden will fire.
  await testShowHideEvent({
    menuCreateParams: {
      title: "any menu item",
      contexts: ["all"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
      viewType: "tab",
      editable: false,
      frameId: 0,
    },
    async doOpenMenu() {
      await openContextMenu("body");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });

  // Now the menu has been shown and hidden, and in another extension the
  // onShown/onHidden events have been dispatched.
  extension.sendMessage("check menu events");
  let events = await extension.awaitMessage("events from menuless extension");
  is(events.length, 2, "expect two events");
  is(events[1], "onHidden", "last event should be onHidden");
  ok(events[0].targetElementId, "info.targetElementId must be set in onShown");
  delete events[0].targetElementId;
  Assert.deepEqual(
    events[0],
    {
      menuIds: [],
      contexts: ["page", "all"],
      viewType: "tab",
      editable: false,
      pageUrl: PAGE,
      frameId: 0,
    },
    "expected onShown info from menuless extension"
  );
  await extension.unload();
});

add_task(async function test_show_hide_pageAction() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "pageAction item",
      contexts: ["page_action"],
    },
    expectedShownEvent: {
      contexts: ["page_action", "all"],
      viewType: undefined,
      editable: false,
    },
    expectedShownEventWithPermissions: {
      contexts: ["page_action", "all"],
      viewType: undefined,
      editable: false,
      pageUrl: PAGE,
    },
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "page");
    },
    async doCloseMenu() {
      await closeActionContextMenu(null, "page");
    },
  });
});

add_task(async function test_show_hide_browserAction() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "browserAction item",
      contexts: ["browser_action"],
    },
    expectedShownEvent: {
      contexts: ["browser_action", "all"],
      viewType: undefined,
      editable: false,
    },
    expectedShownEventWithPermissions: {
      contexts: ["browser_action", "all"],
      viewType: undefined,
      editable: false,
      pageUrl: PAGE,
    },
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "browser");
    },
    async doCloseMenu() {
      await closeActionContextMenu();
    },
  });
});

add_task(async function test_show_hide_browserAction_v3() {
  await testShowHideEvent({
    manifest_version: 3,
    menuCreateParams: {
      title: "Action item",
      contexts: ["action"],
    },
    expectedShownEvent: {
      contexts: ["action", "all"],
      viewType: undefined,
      editable: false,
    },
    expectedShownEventWithPermissions: {
      contexts: ["action", "all"],
      viewType: undefined,
      editable: false,
      pageUrl: PAGE,
    },
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "browser");
    },
    async doCloseMenu() {
      await closeActionContextMenu();
    },
  });
});

add_task(async function test_show_hide_browserAction_popup() {
  let popupUrl;
  await testShowHideEvent({
    menuCreateParams: {
      title: "browserAction popup - TEST_EXPECT_NO_TAB",
      contexts: ["all", "browser_action"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
      viewType: "popup",
      frameId: 0,
      editable: false,
      get pageUrl() {
        return popupUrl;
      },
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    expectedShownEventWithPermissions: {
      contexts: ["page", "all"],
      viewType: "popup",
      frameId: 0,
      editable: false,
      get pageUrl() {
        return popupUrl;
      },
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu(extension) {
      popupUrl = `moz-extension://${extension.uuid}/popup.html`;
      await clickBrowserAction(extension);
      await openContextMenuInPopup(extension);
    },
    async doCloseMenu(extension) {
      await closeExtensionContextMenu();
      await closeBrowserAction(extension);
    },
  });
});

add_task(async function test_show_hide_browserAction_popup_v3() {
  let popupUrl;
  await testShowHideEvent({
    manifest_version: 3,
    menuCreateParams: {
      title: "Action popup - TEST_EXPECT_NO_TAB",
      contexts: ["all", "action"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
      viewType: "popup",
      frameId: 0,
      editable: false,
      get pageUrl() {
        return popupUrl;
      },
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    expectedShownEventWithPermissions: {
      contexts: ["page", "all"],
      viewType: "popup",
      frameId: 0,
      editable: false,
      get pageUrl() {
        return popupUrl;
      },
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu(extension) {
      popupUrl = `moz-extension://${extension.uuid}/popup.html`;
      await clickBrowserAction(extension);
      await openContextMenuInPopup(extension);
    },
    async doCloseMenu(extension) {
      await closeExtensionContextMenu();
      await closeBrowserAction(extension);
    },
  });
});

// Common code used by test_show_hide_tab and test_show_hide_tab_via_tab_panel.
async function testShowHideTabMenu({
  doOpenTabContextMenu,
  doCloseTabContextMenu,
}) {
  await testShowHideEvent({
    // To verify that the event matches the contextmenu target, switch to
    // an unrelated tab before opening a contextmenu on the desired tab.
    forceTabToBackground: true,
    menuCreateParams: {
      title: "tab menu item",
      contexts: ["tab"],
    },
    expectedShownEvent: {
      contexts: ["tab"],
      viewType: undefined,
      editable: false,
    },
    expectedShownEventWithPermissions: {
      contexts: ["tab"],
      viewType: undefined,
      editable: false,
      pageUrl: PAGE,
    },
    async doOpenMenu(extension, contextTab) {
      await doOpenTabContextMenu(contextTab);
    },
    async doCloseMenu() {
      await doCloseTabContextMenu();
    },
  });
}

add_task(async function test_show_hide_tab() {
  await testShowHideTabMenu({
    async doOpenTabContextMenu(contextTab) {
      await openTabContextMenu(contextTab);
    },
    async doCloseTabContextMenu() {
      await closeTabContextMenu();
    },
  });
});

// Checks that right-clicking on a tab in the tabs panel (the one that appears
// when there are many tabs, or when browser.tabs.tabmanager.enabled = true)
// results in an event that is associated with the expected tab.
add_task(async function test_show_hide_tab_via_tab_panel() {
  gTabsPanel.init();
  const tabContainer = document.getElementById("tabbrowser-tabs");
  let shouldAddOverflow = !tabContainer.hasAttribute("overflow");
  const revertTabContainerAttribute = () => {
    if (shouldAddOverflow) {
      // Revert attribute if it was changed.
      tabContainer.removeAttribute("overflow");
      // The function is going to be called twice, but let's run the logic once.
      shouldAddOverflow = false;
    }
  };
  if (shouldAddOverflow) {
    // Ensure the visibility of the "all tabs menu" button (#alltabs-button).
    tabContainer.setAttribute("overflow", "true");
    // Register cleanup function in case the test fails before we reach the end.
    registerCleanupFunction(revertTabContainerAttribute);
  }

  const allTabsView = document.getElementById("allTabsMenu-allTabsView");

  await testShowHideTabMenu({
    async doOpenTabContextMenu(contextTab) {
      // Show the tabs panel.
      let allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
        allTabsView,
        "ViewShown"
      );
      gTabsPanel.showAllTabsPanel();
      await allTabsPopupShownPromise;

      // Find the menu item that is associated with the given tab
      let index = Array.prototype.findIndex.call(
        gTabsPanel.allTabsViewTabs.children,
        toolbaritem => toolbaritem.tab === contextTab
      );
      ok(index !== -1, "sanity check: tabs panel has item for the tab");

      // Finally, open the context menu on it.
      await openChromeContextMenu(
        "tabContextMenu",
        `.all-tabs-item:nth-child(${index + 1})`
      );
    },
    async doCloseTabContextMenu() {
      await closeTabContextMenu();
      let allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(
        allTabsView.panelMultiView,
        "PanelMultiViewHidden"
      );
      gTabsPanel.hideAllTabsPanel();
      await allTabsPopupHiddenPromise;
    },
  });

  revertTabContainerAttribute();
});

add_task(async function test_show_hide_tools_menu() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "menu item",
      contexts: ["tools_menu"],
    },
    expectedShownEvent: {
      contexts: ["tools_menu"],
      viewType: undefined,
      editable: false,
    },
    expectedShownEventWithPermissions: {
      contexts: ["tools_menu"],
      viewType: undefined,
      editable: false,
      pageUrl: PAGE,
    },
    async doOpenMenu() {
      await openToolsMenu();
    },
    async doCloseMenu() {
      await closeToolsMenu();
    },
  });
});

add_task(async function test_show_hide_page() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "page menu item",
      contexts: ["page"],
    },
    expectedShownEvent: {
      contexts: ["page", "all"],
      viewType: "tab",
      editable: false,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["page", "all"],
      viewType: "tab",
      editable: false,
      pageUrl: PAGE,
      frameId: 0,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await openContextMenu("body");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_frame() {
  // frame info will be determined before opening the menu.
  let frameId;
  await testShowHideEvent({
    menuCreateParams: {
      title: "subframe menu item",
      contexts: ["frame"],
    },
    expectedShownEvent: {
      contexts: ["frame", "all"],
      viewType: "tab",
      editable: false,
      get frameId() {
        return frameId;
      },
    },
    expectedShownEventWithPermissions: {
      contexts: ["frame", "all"],
      viewType: "tab",
      editable: false,
      get frameId() {
        return frameId;
      },
      pageUrl: PAGE,
      frameUrl: PAGE_BASE + "context_frame.html",
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      frameId = await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [],
        function() {
          const { WebNavigationFrames } = ChromeUtils.import(
            "resource://gre/modules/WebNavigationFrames.jsm"
          );

          let { contentWindow } = content.document.getElementById("frame");
          return WebNavigationFrames.getFrameId(contentWindow);
        }
      );
      await openContextMenuInFrame();
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_password() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "password item",
      contexts: ["password"],
    },
    expectedShownEvent: {
      contexts: ["editable", "password", "all"],
      viewType: "tab",
      editable: true,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["editable", "password", "all"],
      viewType: "tab",
      editable: true,
      frameId: 0,
      pageUrl: PAGE,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await openContextMenu("#password");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_link() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "link item",
      contexts: ["link"],
    },
    expectedShownEvent: {
      contexts: ["link", "all"],
      viewType: "tab",
      editable: false,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["link", "all"],
      viewType: "tab",
      editable: false,
      frameId: 0,
      linkText: "Some link",
      linkUrl: PAGE_BASE + "some-link",
      pageUrl: PAGE,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await openContextMenu("#link1");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_image_link() {
  await testShowHideEvent({
    menuCreateParams: {
      title: "image item",
      contexts: ["image"],
    },
    expectedShownEvent: {
      contexts: ["image", "link", "all"],
      viewType: "tab",
      mediaType: "image",
      editable: false,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["image", "link", "all"],
      viewType: "tab",
      mediaType: "image",
      editable: false,
      frameId: 0,
      // Apparently, when a link has no content, its href is used as linkText.
      linkText: PAGE_BASE + "image-around-some-link",
      linkUrl: PAGE_BASE + "image-around-some-link",
      srcUrl: PAGE_BASE + "ctxmenu-image.png",
      pageUrl: PAGE,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await openContextMenu("#img-wrapped-in-link");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_editable_selection() {
  let selectionText;
  await testShowHideEvent({
    menuCreateParams: {
      title: "editable item",
      contexts: ["editable"],
    },
    expectedShownEvent: {
      contexts: ["editable", "selection", "all"],
      viewType: "tab",
      editable: true,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["editable", "selection", "all"],
      viewType: "tab",
      editable: true,
      frameId: 0,
      pageUrl: PAGE,
      get selectionText() {
        return selectionText;
      },
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      // Select lots of text in the test page before opening the menu.
      selectionText = await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [],
        function() {
          let node = content.document.getElementById("editabletext");
          node.select();
          node.focus();
          return node.value;
        }
      );

      await openContextMenu("#editabletext");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_video() {
  const VIDEO_URL = "data:video/webm,xxx";
  await testShowHideEvent({
    menuCreateParams: {
      title: "video item",
      contexts: ["video"],
    },
    expectedShownEvent: {
      contexts: ["video", "all"],
      viewType: "tab",
      mediaType: "video",
      editable: false,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["video", "all"],
      viewType: "tab",
      mediaType: "video",
      editable: false,
      frameId: 0,
      srcUrl: VIDEO_URL,
      pageUrl: PAGE,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [VIDEO_URL], function(
        VIDEO_URL
      ) {
        let video = content.document.createElement("video");
        video.controls = true;
        video.src = VIDEO_URL;
        content.document.body.appendChild(video);
        video.focus();
      });

      await openContextMenu("video");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});

add_task(async function test_show_hide_audio() {
  const AUDIO_URL = "data:audio/ogg,xxx";
  await testShowHideEvent({
    menuCreateParams: {
      title: "audio item",
      contexts: ["audio"],
    },
    expectedShownEvent: {
      contexts: ["audio", "all"],
      viewType: "tab",
      mediaType: "audio",
      editable: false,
      frameId: 0,
    },
    expectedShownEventWithPermissions: {
      contexts: ["audio", "all"],
      viewType: "tab",
      mediaType: "audio",
      editable: false,
      frameId: 0,
      srcUrl: AUDIO_URL,
      pageUrl: PAGE,
      targetElementId: EXPECT_TARGET_ELEMENT,
    },
    async doOpenMenu() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUDIO_URL], function(
        AUDIO_URL
      ) {
        let audio = content.document.createElement("audio");
        audio.controls = true;
        audio.src = AUDIO_URL;
        content.document.body.appendChild(audio);
        audio.focus();
      });

      await openContextMenu("audio");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
});
