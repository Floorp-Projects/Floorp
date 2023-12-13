/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);

let gCUITestUtils = new CustomizableUITestUtils(window);

add_setup(async function () {
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
  // This preference gets set the very first time that the FxA menu gets opened,
  // which can cause a state write to occur, which can confuse this test, since
  // when in the signed-out state, we need to set the state _before_ opening
  // the FxA menu (since the panel cannot be opened) in the signed out state.
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.toolbar.accessed", true]],
  });
});

add_task(async function test_ui_state_notification_calls_updateAllUI() {
  let called = false;
  let updateAllUI = gSync.updateAllUI;
  gSync.updateAllUI = () => {
    called = true;
  };

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  ok(called);

  gSync.updateAllUI = updateAllUI;
});

add_task(async function test_navBar_button_visibility() {
  const button = document.getElementById("fxa-toolbar-menu-button");
  ok(button.closest("#nav-bar"), "button is in the #nav-bar");

  const state = {
    status: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: true,
  };
  gSync.updateAllUI(state);
  ok(
    BrowserTestUtils.is_hidden(button),
    "Button should be hidden with STATUS_NOT_CONFIGURED"
  );

  state.email = "foo@bar.com";
  state.status = UIState.STATUS_NOT_VERIFIED;
  gSync.updateAllUI(state);
  ok(
    BrowserTestUtils.is_visible(button),
    "Check button visibility with STATUS_NOT_VERIFIED"
  );

  state.status = UIState.STATUS_LOGIN_FAILED;
  gSync.updateAllUI(state);
  ok(
    BrowserTestUtils.is_visible(button),
    "Check button visibility with STATUS_LOGIN_FAILED"
  );

  state.status = UIState.STATUS_SIGNED_IN;
  gSync.updateAllUI(state);
  ok(
    BrowserTestUtils.is_visible(button),
    "Check button visibility with STATUS_SIGNED_IN"
  );

  state.syncEnabled = false;
  gSync.updateAllUI(state);
  is(
    BrowserTestUtils.is_visible(button),
    true,
    "Check button visibility when signed in, but sync disabled"
  );
});

add_task(async function test_overflow_navBar_button_visibility() {
  const button = document.getElementById("fxa-toolbar-menu-button");

  let overflowPanel = document.getElementById("widget-overflow");
  overflowPanel.setAttribute("animate", "false");
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  let originalWindowWidth = window.outerWidth;

  registerCleanupFunction(function () {
    overflowPanel.removeAttribute("animate");
    window.resizeTo(originalWindowWidth, window.outerHeight);
    return TestUtils.waitForCondition(
      () => !navbar.hasAttribute("overflowing")
    );
  });

  window.resizeTo(450, window.outerHeight);

  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = BrowserTestUtils.waitForEvent(
    overflowPanel,
    "popupshown"
  );
  chevron.click();
  await shownPanelPromise;

  ok(button, "fxa-toolbar-menu-button was found");

  const state = {
    status: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: true,
  };
  gSync.updateAllUI(state);

  ok(
    BrowserTestUtils.is_hidden(button),
    "Button should be hidden with STATUS_NOT_CONFIGURED"
  );

  let hidePanelPromise = BrowserTestUtils.waitForEvent(
    overflowPanel,
    "popuphidden"
  );
  chevron.click();
  await hidePanelPromise;
});

add_task(async function setupForPanelTests() {
  /* Proton hides the FxA toolbar button when in the nav-bar and unconfigured.
     To test the panel in all states, we move it to the tabstrip toolbar where
     it will always be visible.
   */
  CustomizableUI.addWidgetToArea(
    "fxa-toolbar-menu-button",
    CustomizableUI.AREA_TABSTRIP
  );

  // make sure it gets put back at the end of the tests
  registerCleanupFunction(() => {
    CustomizableUI.addWidgetToArea(
      "fxa-toolbar-menu-button",
      CustomizableUI.AREA_NAVBAR
    );
  });
});

add_task(async function test_ui_state_signedin() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  const relativeDateAnchor = new Date();
  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: false,
  };

  const origRelativeTimeFormat = gSync.relativeTimeFormat;
  gSync.relativeTimeFormat = {
    formatBestUnit(date) {
      return origRelativeTimeFormat.formatBestUnit(date, {
        now: relativeDateAnchor,
      });
    },
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  checkMenuBarItem("sync-syncnowitem");
  checkPanelHeader();
  checkFxaToolbarButtonPanel({
    headerTitle: "Manage account",
    headerDescription: state.displayName,
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-connect-device-button",
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
      "PanelUI-fxa-menu-account-signout-button",
    ],
    disabledItems: [],
    hiddenItems: ["PanelUI-fxa-menu-setup-sync-button"],
  });
  checkFxAAvatar("signedin");
  gSync.relativeTimeFormat = origRelativeTimeFormat;
  await closeFxaPanel();

  await openMainPanel();

  checkPanelUIStatusBar({
    description: "Foo Bar",
    titleHidden: true,
    hideFxAText: true,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_syncing_panel_closed() {
  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: true,
  };

  gSync.updateAllUI(state);

  checkSyncNowButtons(true);

  // Be good citizens and remove the "syncing" state.
  gSync.updateAllUI({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    lastSync: new Date(),
    syncing: false,
  });
  // Because we switch from syncing to non-syncing, and there's a timeout involved.
  await promiseObserver("test:browser-sync:activity-stop");
});

add_task(async function test_ui_state_syncing_panel_open() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: false,
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  checkSyncNowButtons(false);

  state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: true,
  };

  gSync.updateAllUI(state);

  checkSyncNowButtons(true);

  // Be good citizens and remove the "syncing" state.
  gSync.updateAllUI({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    lastSync: new Date(),
    syncing: false,
  });
  // Because we switch from syncing to non-syncing, and there's a timeout involved.
  await promiseObserver("test:browser-sync:activity-stop");

  await closeFxaPanel();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_ui_state_panel_open_after_syncing() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: true,
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  checkSyncNowButtons(true);

  // Be good citizens and remove the "syncing" state.
  gSync.updateAllUI({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "foo@bar.com",
    lastSync: new Date(),
    syncing: false,
  });
  // Because we switch from syncing to non-syncing, and there's a timeout involved.
  await promiseObserver("test:browser-sync:activity-stop");

  await closeFxaPanel();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_ui_state_unconfigured() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_NOT_CONFIGURED,
  };

  gSync.updateAllUI(state);

  checkMenuBarItem("sync-setup");

  checkFxAAvatar("not_configured");

  let signedOffLabel = gSync.fluentStrings.formatValueSync(
    "appmenu-fxa-signed-in-label"
  );

  await openMainPanel();

  checkPanelUIStatusBar({
    description: signedOffLabel,
    titleHidden: true,
    hideFxAText: false,
  });
  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_signed_in() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: false,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  checkMenuBarItem("sync-enable");
  checkPanelHeader();
  checkFxaToolbarButtonPanel({
    headerTitle: "Manage account",
    headerDescription: "Foo Bar",
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-connect-device-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-account-signout-button",
    ],
    disabledItems: [],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("signedin");
  await closeFxaPanel();

  await openMainPanel();

  checkPanelUIStatusBar({
    description: "Foo Bar",
    titleHidden: true,
    hideFxAText: true,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_signed_in_no_display_name() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: false,
    email: "foo@bar.com",
    avatarURL: "https://foo.bar",
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  checkMenuBarItem("sync-enable");
  checkPanelHeader();
  checkFxaToolbarButtonPanel({
    headerTitle: "Manage account",
    headerDescription: "foo@bar.com",
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-connect-device-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-account-signout-button",
    ],
    disabledItems: [],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("signedin");
  await closeFxaPanel();

  await openMainPanel();

  checkPanelUIStatusBar({
    description: "foo@bar.com",
    titleHidden: true,
    hideFxAText: true,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_unverified() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_NOT_VERIFIED,
    email: "foo@bar.com",
    syncing: false,
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  const expectedLabel = gSync.fluentStrings.formatValueSync(
    "account-finish-account-setup"
  );

  checkMenuBarItem("sync-unverifieditem");
  checkPanelHeader();
  checkFxaToolbarButtonPanel({
    headerTitle: expectedLabel,
    headerDescription: state.email,
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-account-signout-button",
    ],
    disabledItems: ["PanelUI-fxa-menu-connect-device-button"],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("unverified");
  await closeFxaPanel();
  await openMainPanel();

  checkPanelUIStatusBar({
    description: state.email,
    title: expectedLabel,
    titleHidden: false,
    hideFxAText: true,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_loginFailed() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let state = {
    status: UIState.STATUS_LOGIN_FAILED,
    email: "foo@bar.com",
    displayName: "Foo Bar",
  };

  gSync.updateAllUI(state);

  await openFxaPanel();

  const expectedLabel = gSync.fluentStrings.formatValueSync(
    "account-disconnected2"
  );

  checkMenuBarItem("sync-reauthitem");
  checkPanelHeader();
  checkFxaToolbarButtonPanel({
    headerTitle: expectedLabel,
    headerDescription: state.displayName,
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-account-signout-button",
    ],
    disabledItems: ["PanelUI-fxa-menu-connect-device-button"],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("login-failed");
  await closeFxaPanel();
  await openMainPanel();

  checkPanelUIStatusBar({
    description: state.displayName,
    title: expectedLabel,
    titleHidden: false,
    hideFxAText: true,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_app_menu_fxa_disabled() {
  const newWin = await BrowserTestUtils.openNewBrowserWindow();

  Services.prefs.setBoolPref("identity.fxaccounts.enabled", true);
  newWin.gSync.onFxaDisabled();

  let menuButton = newWin.document.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(newWin.PanelUI.mainView, "ViewShown");

  [...newWin.document.querySelectorAll(".sync-ui-item")].forEach(
    e => (e.hidden = false)
  );

  let hidden = BrowserTestUtils.waitForEvent(
    newWin.document,
    "popuphidden",
    true
  );
  newWin.PanelUI.hide();
  await hidden;
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(
  // Can't open the history menu in tests on Mac.
  () => AppConstants.platform != "mac",
  async function test_history_menu_fxa_disabled() {
    const newWin = await BrowserTestUtils.openNewBrowserWindow();

    Services.prefs.setBoolPref("identity.fxaccounts.enabled", true);
    newWin.gSync.onFxaDisabled();

    const historyMenubarItem = window.document.getElementById("history-menu");
    const historyMenu = window.document.getElementById("historyMenuPopup");
    const syncedTabsItem = historyMenu.querySelector("#sync-tabs-menuitem");
    const menuShown = BrowserTestUtils.waitForEvent(historyMenu, "popupshown");
    historyMenubarItem.openMenu(true);
    await menuShown;

    Assert.equal(
      syncedTabsItem.hidden,
      true,
      "Synced Tabs item should not be displayed when FxAccounts is disabled"
    );
    const menuHidden = BrowserTestUtils.waitForEvent(
      historyMenu,
      "popuphidden"
    );
    historyMenu.hidePopup();
    await menuHidden;
    await BrowserTestUtils.closeWindow(newWin);
  }
);

function checkPanelUIStatusBar({
  description,
  title,
  titleHidden,
  hideFxAText,
}) {
  checkAppMenuFxAText(hideFxAText);
  let appMenuHeaderTitle = PanelMultiView.getViewNode(
    document,
    "appMenu-header-title"
  );
  let appMenuHeaderDescription = PanelMultiView.getViewNode(
    document,
    "appMenu-header-description"
  );
  is(
    appMenuHeaderDescription.value,
    description,
    "app menu description has correct value"
  );
  is(appMenuHeaderTitle.hidden, titleHidden, "title has correct hidden status");
  if (!titleHidden) {
    is(appMenuHeaderTitle.value, title, "title has correct value");
  }
}

function checkMenuBarItem(expectedShownItemId) {
  checkItemsVisibilities(
    [
      "sync-setup",
      "sync-enable",
      "sync-syncnowitem",
      "sync-reauthitem",
      "sync-unverifieditem",
    ],
    expectedShownItemId
  );
}

function checkPanelHeader() {
  let fxaPanelView = PanelMultiView.getViewNode(document, "PanelUI-fxa");
  is(
    fxaPanelView.getAttribute("title"),
    gSync.fluentStrings.formatValueSync("appmenu-account-header"),
    "Panel title is correct"
  );
}

function checkSyncNowButtons(syncing, tooltip = null) {
  const syncButtons = document.querySelectorAll(".syncNowBtn");

  for (const syncButton of syncButtons) {
    is(
      syncButton.getAttribute("syncstatus"),
      syncing ? "active" : "",
      "button active has the right value"
    );
    if (tooltip) {
      is(
        syncButton.getAttribute("tooltiptext"),
        tooltip,
        "button tooltiptext is set to the right value"
      );
    }
  }

  const syncLabels = document.querySelectorAll(".syncnow-label");

  for (const syncLabel of syncLabels) {
    if (syncing) {
      is(
        syncLabel.getAttribute("data-l10n-id"),
        syncLabel.getAttribute("syncing-data-l10n-id"),
        "label is set to the right value"
      );
    } else {
      is(
        syncLabel.getAttribute("data-l10n-id"),
        syncLabel.getAttribute("sync-now-data-l10n-id"),
        "label is set to the right value"
      );
    }
  }
}

async function checkFxaToolbarButtonPanel({
  headerTitle,
  headerDescription,
  enabledItems,
  disabledItems,
  hiddenItems,
}) {
  is(
    document.getElementById("fxa-menu-header-title").value,
    headerTitle,
    "has correct title"
  );
  is(
    document.getElementById("fxa-menu-header-description").value,
    headerDescription,
    "has correct description"
  );

  for (const id of enabledItems) {
    const el = document.getElementById(id);
    is(el.hasAttribute("disabled"), false, id + " is enabled");
  }

  for (const id of disabledItems) {
    const el = document.getElementById(id);
    is(el.getAttribute("disabled"), "true", id + " is disabled");
  }

  for (const id of hiddenItems) {
    const el = document.getElementById(id);
    is(el.getAttribute("hidden"), "true", id + " is hidden");
  }
}

async function checkFxABadged() {
  const button = document.getElementById("fxa-toolbar-menu-button");
  await BrowserTestUtils.waitForCondition(() => {
    return button.querySelector("label.feature-callout");
  });
  const badge = button.querySelector("label.feature-callout");
  ok(badge, "expected feature-callout style badge");
  ok(BrowserTestUtils.is_visible(badge), "expected the badge to be visible");
}

// fxaStatus is one of 'not_configured', 'unverified', 'login-failed', or 'signedin'.
function checkFxAAvatar(fxaStatus) {
  // Unhide the panel so computed styles can be read
  document.querySelector("#appMenu-popup").hidden = false;

  const avatarContainers = [document.getElementById("fxa-avatar-image")];
  for (const avatar of avatarContainers) {
    const avatarURL = getComputedStyle(avatar).listStyleImage;
    const expected = {
      not_configured: 'url("chrome://browser/skin/fxa/avatar-empty.svg")',
      unverified: 'url("chrome://browser/skin/fxa/avatar.svg")',
      signedin: 'url("chrome://browser/skin/fxa/avatar.svg")',
      "login-failed": 'url("chrome://browser/skin/fxa/avatar.svg")',
    };
    ok(
      avatarURL == expected[fxaStatus],
      `expected avatar URL to be ${expected[fxaStatus]}, got ${avatarURL}`
    );
  }
}

function checkAppMenuFxAText(hideStatus) {
  let fxaText = document.getElementById("appMenu-fxa-text");
  let isHidden = fxaText.hidden || fxaText.style.visibility == "collapse";
  ok(isHidden == hideStatus, "FxA text has correct hidden state");
}

// Only one item visible at a time.
function checkItemsVisibilities(itemsIds, expectedShownItemId) {
  for (let id of itemsIds) {
    if (id == expectedShownItemId) {
      ok(
        !document.getElementById(id).hidden,
        "menuitem " + id + " should be visible"
      );
    } else {
      ok(
        document.getElementById(id).hidden,
        "menuitem " + id + " should be hidden"
      );
    }
  }
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    };
    Services.obs.addObserver(obs, topic);
  });
}

async function openTabAndFxaPanel() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");
  await openFxaPanel();
}

async function openFxaPanel() {
  let fxaButton = document.getElementById("fxa-toolbar-menu-button");
  fxaButton.click();

  let fxaView = PanelMultiView.getViewNode(document, "PanelUI-fxa");
  await BrowserTestUtils.waitForEvent(fxaView, "ViewShown");
}

async function closeFxaPanel() {
  let fxaView = PanelMultiView.getViewNode(document, "PanelUI-fxa");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  fxaView.closest("panel").hidePopup();
  await hidden;
}

async function openMainPanel() {
  let menuButton = document.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");
}

async function closeTabAndMainPanel() {
  await gCUITestUtils.hideMainMenu();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}
