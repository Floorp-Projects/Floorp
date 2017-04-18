/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_ui_state_notification_calls_updateAllUI() {
  let called = false;
  let updateAllUI = gSync.updateAllUI;
  gSync.updateAllUI = () => { called = true; };

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  ok(called);

  gSync.updateAllUI = updateAllUI;
});

add_task(async function test_ui_state_signedin() {
  let state = {
    status: UIState.STATUS_SIGNED_IN,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: false
  };

  gSync.updateAllUI(state);

  checkFxABadge(false);
  let statusBarTooltip = gSync.panelUIStatus.getAttribute("signedinTooltiptext");
  let lastSyncTooltip = gSync.formatLastSyncDate(new Date(state.lastSync));
  checkPanelUIStatusBar({
    label: "Foo Bar",
    tooltip: statusBarTooltip,
    fxastatus: "signedin",
    avatarURL: "https://foo.bar",
    syncing: false,
    syncNowTooltip: lastSyncTooltip
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-main", false);
  checkMenuBarItem("sync-syncnowitem");
});

add_task(async function test_ui_state_syncing() {
  let state = {
    status: UIState.STATUS_SIGNED_IN,
    email: "foo@bar.com",
    displayName: "Foo Bar",
    avatarURL: "https://foo.bar",
    lastSync: new Date(),
    syncing: true
  };

  gSync.updateAllUI(state);

  checkSyncNowButton("PanelUI-fxa-icon", true);
  checkSyncNowButton("PanelUI-remotetabs-syncnow", true);

  // Be good citizens and remove the "syncing" state.
  gSync.updateAllUI({
    status: UIState.STATUS_SIGNED_IN,
    email: "foo@bar.com",
    lastSync: new Date(),
    syncing: false
  });
  // Because we switch from syncing to non-syncing, and there's a timeout involved.
  await promiseObserver("test:browser-sync:activity-stop");
});

add_task(async function test_ui_state_unconfigured() {
  let state = {
    status: UIState.STATUS_NOT_CONFIGURED
  };

  gSync.updateAllUI(state);

  checkFxABadge(false);
  let signedOffLabel = gSync.panelUIStatus.getAttribute("defaultlabel");
  let statusBarTooltip = gSync.panelUIStatus.getAttribute("signedinTooltiptext");
  checkPanelUIStatusBar({
    label: signedOffLabel,
    tooltip: statusBarTooltip
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-setupsync");
  checkMenuBarItem("sync-setup");
});

add_task(async function test_ui_state_unverified() {
  let state = {
    status: UIState.STATUS_NOT_VERIFIED,
    email: "foo@bar.com",
    lastSync: new Date(),
    syncing: false
  };

  gSync.updateAllUI(state);

  checkFxABadge(true);
  let expectedLabel = gSync.panelUIStatus.getAttribute("unverifiedlabel");
  let tooltipText = gSync.fxaStrings.formatStringFromName("verifyDescription", [state.email], 1);
  checkPanelUIStatusBar({
    label: expectedLabel,
    tooltip: tooltipText,
    fxastatus: "unverified",
    avatarURL: null,
    syncing: false,
    syncNowTooltip: tooltipText
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-setupsync", false);
  checkMenuBarItem("sync-setup");
});

add_task(async function test_ui_state_loginFailed() {
  let state = {
    status: UIState.STATUS_LOGIN_FAILED,
    email: "foo@bar.com"
  };

  gSync.updateAllUI(state);

  checkFxABadge(true);
  let expectedLabel = gSync.panelUIStatus.getAttribute("errorlabel");
  let tooltipText = gSync.fxaStrings.formatStringFromName("reconnectDescription", [state.email], 1);
  checkPanelUIStatusBar({
    label: expectedLabel,
    tooltip: tooltipText,
    fxastatus: "login-failed",
    avatarURL: null,
    syncing: false,
    syncNowTooltip: tooltipText
  });
  checkRemoteTabsPanel("PanelUI-remotetabs-reauthsync", false);
  checkMenuBarItem("sync-reauthitem");
});

add_task(async function test_FormatLastSyncDateNow() {
  let now = new Date();
  let nowString = gSync.formatLastSyncDate(now);
  is(nowString, "Last sync: " + now.toLocaleDateString(undefined, {weekday: "long", hour: "numeric", minute: "numeric"}),
     "The date is correctly formatted");
});

add_task(async function test_FormatLastSyncDateMonthAgo() {
  let monthAgo = new Date();
  monthAgo.setMonth(monthAgo.getMonth() - 1);
  let monthAgoString = gSync.formatLastSyncDate(monthAgo);
  is(monthAgoString, "Last sync: " + monthAgo.toLocaleDateString(undefined, {month: "long", day: "numeric"}),
     "The date is correctly formatted");
});

function checkFxABadge(shouldBeShown) {
  let isShown = false;
  for (let notification of PanelUI.notifications) {
    if (notification.id == "fxa-needs-authentication") {
      isShown = true;
      break;
    }
  }
  is(isShown, shouldBeShown, "the fxa badge has the right visibility");
}

function checkPanelUIStatusBar({label, tooltip, fxastatus, avatarURL, syncing, syncNowTooltip}) {
  let labelNode = document.getElementById("PanelUI-fxa-label");
  let tooltipNode = document.getElementById("PanelUI-fxa-status");
  let statusNode = document.getElementById("PanelUI-footer-fxa");
  let avatar = document.getElementById("PanelUI-fxa-avatar");

  is(labelNode.getAttribute("label"), label, "panelUI-fxa label has the right value");
  is(tooltipNode.getAttribute("tooltiptext"), tooltip, "panelUI-fxa tooltip has the right value");
  if (fxastatus) {
    is(statusNode.getAttribute("fxastatus"), fxastatus, "panelUI-fxa fxastatus has the right value");
  } else {
    ok(!statusNode.hasAttribute("fxastatus"), "panelUI-fxa fxastatus is unset")
  }
  if (avatarURL) {
    is(avatar.style.listStyleImage, `url("${avatarURL}")`, "panelUI-fxa avatar URL is set");
  } else {
    ok(!statusNode.style.listStyleImage, "panelUI-fxa avatar URL is unset");
  }

  if (syncing != undefined && syncNowTooltip != undefined) {
    checkSyncNowButton("PanelUI-fxa-icon", syncing, syncNowTooltip);
  }
}

function checkRemoteTabsPanel(expectedShownItemId, syncing, syncNowTooltip) {
  checkItemsVisiblities(["PanelUI-remotetabs-main",
                         "PanelUI-remotetabs-setupsync",
                         "PanelUI-remotetabs-reauthsync"],
                        expectedShownItemId);

  if (syncing != undefined && syncNowTooltip != undefined) {
    checkSyncNowButton("PanelUI-remotetabs-syncnow", syncing, syncNowTooltip);
  }
}

function checkMenuBarItem(expectedShownItemId) {
  checkItemsVisiblities(["sync-setup", "sync-syncnowitem", "sync-reauthitem"],
                        expectedShownItemId);
}

function checkSyncNowButton(buttonId, syncing, tooltip = null) {
  const remoteTabsButton = document.getElementById(buttonId);

  is(remoteTabsButton.getAttribute("syncstatus"), syncing ? "active" : "", "button active has the right value");
  if (tooltip) {
    is(remoteTabsButton.getAttribute("tooltiptext"), tooltip, "button tooltiptext is set to the right value");
  }

  if (buttonId == "PanelUI-fxa-icon") {
    return;
  }

  is(remoteTabsButton.hasAttribute("disabled"), syncing, "disabled has the right value");
  if (syncing) {
    is(remoteTabsButton.getAttribute("label"), gSync.syncStrings.GetStringFromName("syncing2.label"), "label is set to the right value");
  } else {
    is(remoteTabsButton.getAttribute("label"), gSync.syncStrings.GetStringFromName("syncnow.label"), "label is set to the right value");
  }
}

// Only one item visible at a time.
function checkItemsVisiblities(itemsIds, expectedShownItemId) {
  for (let id of itemsIds) {
    if (id == expectedShownItemId) {
      ok(!document.getElementById(id).hidden, "menuitem " + id + " should be visible");
    } else {
      ok(document.getElementById(id).hidden, "menuitem " + id + " should be hidden");
    }
  }
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    }
    Services.obs.addObserver(obs, topic);
  });
}
