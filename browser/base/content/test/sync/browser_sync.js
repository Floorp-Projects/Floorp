/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);

let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function setup() {
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
  if (PanelUI.protonAppMenuEnabled) {
    // This preference gets set the very first time that the FxA menu gets opened,
    // which can cause a state write to occur, which can confuse this test in the
    // Proton AppMenu case, since when in the signed-out state, we need to set
    // the state _before_ opening the FxA menu (since the panel cannot be opened)
    // in the signed out state.
    await SpecialPowers.pushPrefEnv({
      set: [["identity.fxaccounts.toolbar.accessed", true]],
    });
  }
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
  info("proton enabled: " + CustomizableUI.protonToolbarEnabled);

  ok(button.closest("#nav-bar"), "button is in the #nav-bar");

  const state = {
    status: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: true,
  };
  gSync.updateAllUI(state);
  is(
    BrowserTestUtils.is_visible(button),
    !CustomizableUI.protonToolbarEnabled,
    "Check button visibility with STATUS_NOT_CONFIGURED"
  );

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

  if (!PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is not enabled, then the Firefox Accounts panel
    // can be opened while in the signed-out state.
    await openFxaPanel();
  }

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

  if (PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is enabled, then at this point we can open the
    // Firefox Accounts panel.
    await openFxaPanel();
  }

  checkMenuBarItem("sync-syncnowitem");
  checkFxaToolbarButtonPanel({
    headerTitle: "Manage Account",
    headerDescription: "foo@bar.com",
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
    label: "foo@bar.com",
    fxastatus: "signedin",
    syncing: false,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_syncing() {
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

add_task(async function test_ui_state_unconfigured() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  if (!PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is not enabled, then the Firefox Accounts panel
    // can be opened while in the signed-out state.
    await openFxaPanel();
  }

  let state = {
    status: UIState.STATUS_NOT_CONFIGURED,
  };

  gSync.updateAllUI(state);

  let appMenuStatusID = PanelUI.protonAppMenuEnabled
    ? "appMenu-fxa-status2"
    : "appMenu-fxa-status";
  let appMenuStatus = PanelMultiView.getViewNode(document, appMenuStatusID);

  checkMenuBarItem("sync-setup");

  // With Proton not enabled, it's possible to open the FxA Panel to see the
  // signed out state.
  if (!PanelUI.protonAppMenuEnabled) {
    let signedOffLabel = appMenuStatus.getAttribute("defaultlabel");
    checkPanelUIStatusBar({
      label: signedOffLabel,
    });
    checkFxaToolbarButtonPanel({
      headerTitle: signedOffLabel,
      headerDescription: "Turn on Sync",
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
  }
  checkFxAAvatar("not_configured");

  if (!PanelUI.protonAppMenuEnabled) {
    await closeFxaPanel();
    await openMainPanel();

    let signedOffLabel = appMenuStatus.getAttribute("defaultlabel");
    checkPanelUIStatusBar({
      label: signedOffLabel,
    });
    await closeTabAndMainPanel();
  } else {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function test_ui_state_syncdisabled() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  if (!PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is not enabled, then the Firefox Accounts panel
    // can be opened while in the signed-out state.
    await openFxaPanel();
  }

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: false,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
  };

  gSync.updateAllUI(state);

  if (PanelUI.protonAppMenuEnabled) {
    // If Proton AppMenu is enabled, we can now open the panel now that
    // it thinks we're signed in.
    await openFxaPanel();
  }

  checkMenuBarItem("sync-enable");
  checkFxaToolbarButtonPanel({
    headerTitle: "Manage Account",
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
    label: "foo@bar.com",
    fxastatus: "signedin",
    syncing: false,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_unverified() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  if (!PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is not enabled, then the Firefox Accounts panel
    // can be opened while in the signed-out state.
    await openFxaPanel();
  }

  let state = {
    status: UIState.STATUS_NOT_VERIFIED,
    email: "foo@bar.com",
    syncing: false,
  };

  gSync.updateAllUI(state);

  if (PanelUI.protonAppMenuEnabled) {
    // If Proton AppMenu is enabled, we can now open the panel now that
    // it thinks we're signed in.
    await openFxaPanel();
  }

  const expectedLabel = gSync.fxaStrings.GetStringFromName(
    "account.finishAccountSetup"
  );

  checkMenuBarItem("sync-unverifieditem");
  checkFxaToolbarButtonPanel({
    headerTitle: state.email,
    headerDescription: expectedLabel,
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
    label: expectedLabel,
    fxastatus: "unverified",
    syncing: false,
  });

  await closeTabAndMainPanel();
});

add_task(async function test_ui_state_loginFailed() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  if (!PanelUI.protonAppMenuEnabled) {
    // If the Proton AppMenu is not enabled, then the Firefox Accounts panel
    // can be opened while in the signed-out state.
    await openFxaPanel();
  }

  let state = {
    status: UIState.STATUS_LOGIN_FAILED,
    email: "foo@bar.com",
  };

  gSync.updateAllUI(state);

  if (PanelUI.protonAppMenuEnabled) {
    // If Proton AppMenu is enabled, we can now open the panel now that
    // it thinks we're signed in.
    await openFxaPanel();
  }

  const expectedLabel = gSync.fxaStrings.GetStringFromName(
    "account.reconnectToFxA"
  );

  checkMenuBarItem("sync-reauthitem");
  checkFxaToolbarButtonPanel({
    headerTitle: state.email,
    headerDescription: expectedLabel,
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
    label: expectedLabel,
    fxastatus: "login-failed",
    syncing: false,
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

  // With Proton disabled, when signed out, the appMenu-fxa-status is hidden.
  // With Proton enabled, it's re-purposed as a "Sign in" button.
  if (!PanelUI.protonAppMenuEnabled) {
    ok(
      BrowserTestUtils.is_hidden(
        newWin.document.getElementById("appMenu-fxa-status")
      ),
      "Fxa status is hidden"
    );
  }

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

function checkPanelUIStatusBar({ label, fxastatus, syncing }) {
  let labelID = PanelUI.protonAppMenuEnabled
    ? "appMenu-fxa-label2"
    : "appMenu-fxa-label";
  let labelNode = PanelMultiView.getViewNode(document, labelID);
  is(labelNode.getAttribute("label"), label, "fxa label has the right value");
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
        syncLabel.value,
        gSync.fluentStrings.formatValueSync("fxa-toolbar-sync-syncing2"),
        "label is set to the right value"
      );
    } else {
      is(
        syncLabel.value,
        gSync.fluentStrings.formatValueSync(
          "appmenuitem-fxa-toolbar-sync-now2"
        ),
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
    let elShown = window.getComputedStyle(el).display == "none";
    is(elShown, true, id + " is hidden");
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

  const avatarContainers = [
    PanelMultiView.getViewNode(document, "fxa-menu-avatar"),
    document.getElementById("fxa-avatar-image"),
  ];
  for (const avatar of avatarContainers) {
    const avatarURL = getComputedStyle(avatar).listStyleImage;
    const expected = {
      not_configured: 'url("chrome://browser/skin/fxa/avatar-empty.svg")',
      unverified: 'url("chrome://browser/skin/fxa/avatar-confirm.svg")',
      signedin: 'url("chrome://browser/skin/fxa/avatar.svg")',
      "login-failed": 'url("chrome://browser/skin/fxa/avatar-alert.svg")',
    };
    ok(
      avatarURL == expected[fxaStatus],
      `expected avatar URL to be ${expected[fxaStatus]}, got ${avatarURL}`
    );
  }
}

// Only one item displayed at a time.
function checkItemsDisplayed(itemsIds, expectedShownItemId) {
  for (let id of itemsIds) {
    if (id == expectedShownItemId) {
      ok(
        BrowserTestUtils.is_visible(document.getElementById(id)),
        `view ${id} should be visible`
      );
    } else {
      ok(
        BrowserTestUtils.is_hidden(document.getElementById(id)),
        `view ${id} should be hidden`
      );
    }
  }
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
