/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");

const fxaDevices = [
  {
    id: 1,
    name: "Foo",
    availableCommands: { "https://identity.mozilla.com/cmd/open-uri": "baz" },
  },
  { id: 2, name: "Bar", clientRecord: "bar" }, // Legacy send tab target (no availableCommands).
  { id: 3, name: "Homer" }, // Incompatible target.
];

add_task(async function setup() {
  await promiseSyncReady();
  await Services.search.init();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
  sinon
    .stub(Weave.Service.clientsEngine, "getClientByFxaDeviceId")
    .callsFake(fxaDeviceId => {
      let target = fxaDevices.find(c => c.id == fxaDeviceId);
      return target ? target.clientRecord : null;
    });
  sinon.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
});

add_task(async function test_page_contextmenu() {
  const sandbox = setupSendTabMocks({ fxaDevices });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    false,
    "Send page to device is shown"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup([
    { label: "Bar" },
    { label: "Foo" },
    "----",
    { label: "Send to All Devices" },
    { label: "Manage Devices..." },
  ]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_link_contextmenu() {
  const sandbox = setupSendTabMocks({ fxaDevices });
  let expectation = sandbox
    .mock(gSync)
    .expects("sendTabToDevice")
    .once()
    .withExactArgs(
      "https://www.example.org/",
      [fxaDevices[1]],
      "Click on me!!"
    );

  // Add a link to the page
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let a = content.document.createElement("a");
    a.href = "https://www.example.org";
    a.id = "testingLink";
    a.textContent = "Click on me!!";
    content.document.body.appendChild(a);
  });

  await openContentContextMenu(
    "#testingLink",
    "context-sendlinktodevice",
    "context-sendlinktodevice-popup"
  );

  let expectedArray = ["context-openlinkintab"];

  if (
    Services.prefs.getBoolPref("privacy.userContext.enabled") &&
    ContextualIdentityService.getPublicIdentities().length
  ) {
    expectedArray.push("context-openlinkinusercontext-menu");
  }

  expectedArray.push(
    "context-openlink",
    "context-openlinkprivate",
    "context-sep-open",
    "context-bookmarklink",
    "context-savelink",
    "context-savelinktopocket",
    "context-copylink",
    "context-sendlinktodevice",
    "context-sep-sendlinktodevice",
    "context-searchselect",
    "frame-sep"
  );

  if (
    Services.prefs.getBoolPref("devtools.accessibility.enabled", true) &&
    (Services.prefs.getBoolPref("devtools.everOpened", false) ||
      Services.prefs.getIntPref("devtools.selfxss.count", 0) > 0)
  ) {
    expectedArray.push("context-inspect-a11y");
  }

  expectedArray.push("context-inspect");

  let menu = document.getElementById("contentAreaContextMenu");

  for (let i = 0, j = 0; i < menu.children.length; i++) {
    let item = menu.children[i];
    if (item.hidden) {
      continue;
    }
    Assert.equal(
      item.id,
      expectedArray[j],
      "Ids in context menu match expected values"
    );
    j++;
  }

  is(
    document.getElementById("context-sendlinktodevice").hidden,
    false,
    "Send link to device is shown"
  );
  is(
    document.getElementById("context-sendlinktodevice").disabled,
    false,
    "Send link to device is enabled"
  );
  document
    .getElementById("context-sendlinktodevice-popup")
    .querySelector("menuitem")
    .click();
  await hideContentContextMenu();

  expectation.verify();
  sandbox.restore();
});

add_task(async function test_page_contextmenu_no_remote_clients() {
  const sandbox = setupSendTabMocks({ fxaDevices: [] });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send tab to device is enabled"
  );
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_one_remote_client() {
  const sandbox = setupSendTabMocks({
    fxaDevices: [
      {
        id: 1,
        name: "Foo",
        availableCommands: {
          "https://identity.mozilla.com/cmd/open-uri": "baz",
        },
      },
    ],
  });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    false,
    "Send page to device is shown"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup([{ label: "Foo" }]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_sendable() {
  const sandbox = setupSendTabMocks({ fxaDevices, isSendableURI: false });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    true,
    "Send page to device is disabled"
  );
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_synced_yet() {
  const sandbox = setupSendTabMocks({ fxaDevices: null });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    true,
    "Send page to device is disabled"
  );
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_sync_not_ready_configured() {
  const sandbox = setupSendTabMocks({ syncReady: false });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    true,
    "Send page to device is disabled"
  );
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_sync_not_ready_other_state() {
  const sandbox = setupSendTabMocks({
    syncReady: false,
    state: UIState.STATUS_NOT_VERIFIED,
  });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_unconfigured() {
  const sandbox = setupSendTabMocks({ state: UIState.STATUS_NOT_CONFIGURED });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup();

  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_verified() {
  const sandbox = setupSendTabMocks({ state: UIState.STATUS_NOT_VERIFIED });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup();

  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_login_failed() {
  const sandbox = setupSendTabMocks({ state: UIState.STATUS_LOGIN_FAILED });

  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  is(
    document.getElementById("context-sendpagetodevice").disabled,
    false,
    "Send page to device is enabled"
  );
  checkPopup();

  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_fxa_disabled() {
  const getter = sinon.stub(gSync, "FXA_ENABLED").get(() => false);
  gSync.onFxaDisabled(); // Would have been called on gSync initialization if FXA_ENABLED had been set.
  await openContentContextMenu("#moztext");
  is(
    document.getElementById("context-sendpagetodevice").hidden,
    true,
    "Send page to device is hidden"
  );
  await hideContentContextMenu();
  getter.restore();
  [...document.querySelectorAll(".sync-ui-item")].forEach(
    e => (e.hidden = false)
  );
});

// We are not going to bother testing the visibility of context-sendlinktodevice
// since it uses the exact same code.
// However, browser_contextmenu.js contains tests that verify its presence.

add_task(async function teardown() {
  Weave.Service.clientsEngine.getClientByFxaDeviceId.restore();
  Weave.Service.clientsEngine.getClientType.restore();
  gBrowser.removeCurrentTab();
});

function checkPopup(expectedItems = null) {
  const popup = document.getElementById("context-sendpagetodevice-popup");
  if (!expectedItems) {
    is(popup.state, "closed", "Popup should be hidden.");
    return;
  }
  const menuItems = popup.children;
  for (let i = 0; i < menuItems.length; i++) {
    const menuItem = menuItems[i];
    const expectedItem = expectedItems[i];
    if (expectedItem === "----") {
      is(menuItem.nodeName, "menuseparator", "Found a separator");
      continue;
    }
    is(menuItem.nodeName, "menuitem", "Found a menu item");
    // Bug workaround, menuItem.label "…" encoding is different than ours.
    is(
      menuItem.label.normalize("NFKC"),
      expectedItem.label,
      "Correct menu item label"
    );
    is(
      menuItem.disabled,
      !!expectedItem.disabled,
      "Correct menu item disabled state"
    );
  }
  // check the length last - the above loop might have given us other clues...
  is(
    menuItems.length,
    expectedItems.length,
    "Popup has the expected children count."
  );
}

async function openContentContextMenu(selector, openSubmenuId = null) {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");

  const awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    selector,
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
      shiftkey: false,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;

  if (openSubmenuId) {
    const menu = document.getElementById(openSubmenuId);
    const menuPopup = menu.menupopup;
    const menuPopupPromise = BrowserTestUtils.waitForEvent(
      menuPopup,
      "popupshown"
    );
    menu.openMenu(true);
    await menuPopupPromise;
  }
}

async function hideContentContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await awaitPopupHidden;
}
