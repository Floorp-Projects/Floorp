/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function setup() {
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
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

add_task(async function test_ui_state_signedin() {
  await openTabAndPanel();

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
  checkPanelUIStatusBar({
    label: "foo@bar.com",
    fxastatus: "signedin",
    syncing: false,
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-main", false);
  checkMenuBarItem("sync-syncnowitem");
  checkFxaToolbarButtonPanel({
    headerTitle: "foo@bar.com",
    headerDescription: "Account Settings",
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-connect-device-button",
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-remotetabs-button",
      "PanelUI-fxa-menu-sync-prefs-button",
      "PanelUI-fxa-menu-logins-button",
      "PanelUI-fxa-menu-monitor-button",
      "PanelUI-fxa-menu-send-button",
    ],
    disabledItems: [],
    hiddenItems: ["PanelUI-fxa-menu-setup-sync-button"],
  });
  checkFxAAvatar("signedin");
  gSync.relativeTimeFormat = origRelativeTimeFormat;
  await closeTabAndPanel();
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
  await openTabAndPanel();

  let state = {
    status: UIState.STATUS_NOT_CONFIGURED,
  };

  gSync.updateAllUI(state);

  let signedOffLabel = gSync.appMenuStatus.getAttribute("defaultlabel");
  checkPanelUIStatusBar({
    label: signedOffLabel,
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-setupsync");
  checkMenuBarItem("sync-setup");
  checkFxaToolbarButtonPanel({
    headerTitle: signedOffLabel,
    headerDescription: "Turn on Sync",
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-remotetabs-button",
      "PanelUI-fxa-menu-logins-button",
      "PanelUI-fxa-menu-monitor-button",
      "PanelUI-fxa-menu-send-button",
    ],
    disabledItems: ["PanelUI-fxa-menu-connect-device-button"],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("not_configured");
  await closeTabAndPanel();
});

add_task(async function test_ui_state_syncdisabled() {
  await openTabAndPanel();

  let state = {
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: false,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
  };

  gSync.updateAllUI(state);
  checkPanelUIStatusBar({
    label: "foo@bar.com",
    fxastatus: "signedin",
    syncing: false,
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-syncdisabled", false);
  checkMenuBarItem("sync-enable");
  checkFxaToolbarButtonPanel({
    headerTitle: "foo@bar.com",
    headerDescription: "Account Settings",
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-connect-device-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-remotetabs-button",
      "PanelUI-fxa-menu-logins-button",
      "PanelUI-fxa-menu-monitor-button",
      "PanelUI-fxa-menu-send-button",
    ],
    disabledItems: [],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("signedin");
  await closeTabAndPanel();
});

add_task(async function test_ui_state_unverified() {
  await openTabAndPanel();

  let state = {
    status: UIState.STATUS_NOT_VERIFIED,
    email: "foo@bar.com",
    syncing: false,
  };

  gSync.updateAllUI(state);

  const expectedLabel = gSync.fxaStrings.GetStringFromName(
    "account.finishAccountSetup"
  );
  checkPanelUIStatusBar({
    label: expectedLabel,
    fxastatus: "unverified",
    syncing: false,
  });

  checkRemoteTabsPanel("PanelUI-remotetabs-unverified", false);
  checkMenuBarItem("sync-unverifieditem");
  checkFxaToolbarButtonPanel({
    headerTitle: expectedLabel,
    headerDescription: state.email,
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-remotetabs-button",
      "PanelUI-fxa-menu-logins-button",
      "PanelUI-fxa-menu-monitor-button",
      "PanelUI-fxa-menu-send-button",
    ],
    disabledItems: ["PanelUI-fxa-menu-connect-device-button"],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("unverified");
  await closeTabAndPanel();
});

add_task(async function test_ui_state_loginFailed() {
  await openTabAndPanel();

  let state = {
    status: UIState.STATUS_LOGIN_FAILED,
    email: "foo@bar.com",
  };

  gSync.updateAllUI(state);

  const expectedLabel = gSync.fxaStrings.GetStringFromName(
    "account.reconnectToFxA"
  );

  checkPanelUIStatusBar({
    label: expectedLabel,
    fxastatus: "login-failed",
    syncing: false,
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-reauthsync", false);
  checkMenuBarItem("sync-reauthitem");
  checkFxaToolbarButtonPanel({
    headerTitle: expectedLabel,
    headerDescription: state.email,
    enabledItems: [
      "PanelUI-fxa-menu-sendtab-button",
      "PanelUI-fxa-menu-setup-sync-button",
      "PanelUI-fxa-menu-remotetabs-button",
      "PanelUI-fxa-menu-logins-button",
      "PanelUI-fxa-menu-monitor-button",
      "PanelUI-fxa-menu-send-button",
    ],
    disabledItems: ["PanelUI-fxa-menu-connect-device-button"],
    hiddenItems: [
      "PanelUI-fxa-menu-syncnow-button",
      "PanelUI-fxa-menu-sync-prefs-button",
    ],
  });
  checkFxAAvatar("login-failed");
  await closeTabAndPanel();
});

function checkPanelUIStatusBar({ label, fxastatus, syncing }) {
  let labelNode = document.getElementById("appMenu-fxa-label");
  is(labelNode.getAttribute("label"), label, "fxa label has the right value");
}

function checkRemoteTabsPanel(expectedShownItemId, syncing, syncNowTooltip) {
  checkItemsVisibilities(
    [
      "PanelUI-remotetabs-main",
      "PanelUI-remotetabs-setupsync",
      "PanelUI-remotetabs-reauthsync",
      "PanelUI-remotetabs-unverified",
    ],
    expectedShownItemId
  );

  if (syncing != undefined && syncNowTooltip != undefined) {
    checkSyncNowButtons(syncing, syncNowTooltip);
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

    is(
      syncButton.hasAttribute("disabled"),
      syncing,
      "disabled has the right value"
    );
    if (syncing) {
      is(
        document.l10n.getAttributes(syncButton).id,
        syncButton.getAttribute("syncinglabel"),
        "label is set to the right value"
      );
    } else {
      is(
        document.l10n.getAttributes(syncButton).id,
        "fxa-toolbar-sync-now",
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
    document.getElementById("fxa-menu-avatar"),
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

add_task(async function test_account_settings_state_signedin() {
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
  checkPanelUIStatusBar({
    label: "foo@bar.com",
    fxastatus: "signedin",
    syncing: false,
  });

  await checkAccountPanel([
    "PanelUI-fxa-menu-account-settings-button",
    "PanelUI-fxa-menu-account-signout-button",
  ]);
  await closeTabAndPanel();
});

async function checkAccountPanel(enabledItems) {
  let fxaButton = document.getElementById("fxa-toolbar-menu-button");
  fxaButton.click();

  let fxaView = document.getElementById("PanelUI-fxa");
  await BrowserTestUtils.waitForEvent(fxaView, "ViewShown");

  let manageAccountButton = document.getElementById(
    "fxa-manage-account-button"
  );
  PanelUI.showSubView("PanelUI-fxa-menu-account-panel", manageAccountButton);

  let manageAccountPanel = document.getElementById(
    "PanelUI-fxa-menu-account-panel"
  );
  await BrowserTestUtils.waitForEvent(manageAccountPanel, "ViewShown");

  for (const id of enabledItems) {
    const el = document.getElementById(id);
    is(el.hasAttribute("disabled"), false, id + " is enabled");
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

async function openTabAndPanel() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/");

  let fxaButton = document.getElementById("fxa-toolbar-menu-button");
  fxaButton.click();

  let fxaView = document.getElementById("PanelUI-fxa");
  await BrowserTestUtils.waitForEvent(fxaView, "ViewShown");

  let remoteTabsButton = document.getElementById(
    "PanelUI-fxa-menu-remotetabs-button"
  );
  remoteTabsButton.click();

  let remoteTabsView = document.getElementById("PanelUI-remotetabs");
  await BrowserTestUtils.waitForEvent(remoteTabsView, "ViewShown");
}

async function closeTabAndPanel() {
  let fxaView = document.getElementById("PanelUI-fxa");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  fxaView.closest("panel").hidePopup();
  await hidden;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}
