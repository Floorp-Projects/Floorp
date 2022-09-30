/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const EXAMPLE_COM_URL =
  "https://example.com/document-builder.sjs?html=<h1>Test midi permission with synthetic site permission addon</h1>";
const PAGE_WITH_IFRAMES_URL = `https://example.org/document-builder.sjs?html=
  <h1>Test midi permission with synthetic site permission addon in iframes</h1>
  <iframe id=sameOrigin src="${encodeURIComponent(
    'https://example.org/document-builder.sjs?html=SameOrigin"'
  )}"></iframe>
  <iframe id=crossOrigin  src="${encodeURIComponent(
    'https://example.net/document-builder.sjs?html=CrossOrigin"'
  )}"></iframe>`;

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.webmidi.enabled", true],
      ["dom.sitepermsaddon-provider.enabled", true],
      ["midi.prompt.testing", false],
    ],
  });

  registerCleanupFunction(async () => {
    // Remove the permission.
    await SpecialPowers.removePermission("midi-sysex", {
      url: EXAMPLE_COM_URL,
    });
    await SpecialPowers.removePermission("midi-sysex", {
      url: PAGE_WITH_IFRAMES_URL,
    });
    await SpecialPowers.removePermission("install", {
      url: EXAMPLE_COM_URL,
    });
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  });
});

add_task(async function testRequestMIDIAccess() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, EXAMPLE_COM_URL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  info("Check that midi-sysex isn't set");
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.UNKNOWN_ACTION,
      { url: EXAMPLE_COM_URL }
    ),
    "midi-sysex value should have UNKNOWN permission"
  );

  info("Request midi access");
  let onAddonInstallBlockedNotification = waitForNotification(
    "addon-install-blocked"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.midiAccessRequestPromise = content.wrappedJSObject.navigator.requestMIDIAccess(
      {
        sysex: true,
      }
    );
  });

  info("Deny site permission addon install");
  let addonInstallPanel = await onAddonInstallBlockedNotification;
  let notification = addonInstallPanel.childNodes[0];
  // secondaryButton is the "Don't allow" button
  notification.secondaryButton.click();

  let rejectionMessage = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      let errorMessage;
      try {
        await content.midiAccessRequestPromise;
      } catch (e) {
        errorMessage = `${e.name}: ${e.message}`;
      }

      delete content.midiAccessRequestPromise;
      return errorMessage;
    }
  );
  is(
    rejectionMessage,
    "SecurityError: WebMIDI requires a site permission add-on to activate"
  );

  info("Request midi access again");
  onAddonInstallBlockedNotification = waitForNotification(
    "addon-install-blocked"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.midiAccessRequestPromise = content.wrappedJSObject.navigator.requestMIDIAccess(
      {
        sysex: true,
      }
    );
  });

  info("Accept site permission addon install");
  addonInstallPanel = await onAddonInstallBlockedNotification;
  notification = addonInstallPanel.childNodes[0];
  const dialogPromise = waitForInstallDialog();
  notification.button.click();
  let installDialog = await dialogPromise;
  installDialog.button.click();

  info("Wait for the midi access request promise to resolve");
  const accessGranted = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      try {
        await content.midiAccessRequestPromise;
        return true;
      } catch (e) {}

      delete content.midiAccessRequestPromise;
      return false;
    }
  );
  ok(accessGranted, "requestMIDIAccess resolved");

  info("Check that midi-sysex is now set");
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.ALLOW_ACTION,
      { url: EXAMPLE_COM_URL }
    ),
    "midi-sysex value should have ALLOW permission"
  );

  info("Check that we don't prompt user again once they installed the addon");
  const accessPromiseState = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.wrappedJSObject.navigator
        .requestMIDIAccess()
        .then(() => "resolved");
    }
  );
  is(
    accessPromiseState,
    "resolved",
    "requestMIDIAccess resolved without user prompt"
  );

  info("Check that we don't prompt user again when they perm denied");
  // remove permission to have a clean state
  await SpecialPowers.removePermission("midi-sysex", {
    url: EXAMPLE_COM_URL,
  });

  onAddonInstallBlockedNotification = waitForNotification(
    "addon-install-blocked"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.midiAccessRequestPromise = content.wrappedJSObject.navigator.requestMIDIAccess(
      {
        sysex: true,
      }
    );
  });

  info("Perm-deny site permission addon install");
  addonInstallPanel = await onAddonInstallBlockedNotification;
  // Click the "Never allow" menuitem
  notification.menupopup.querySelector("menuitem").click();

  rejectionMessage = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      let errorMessage;
      try {
        await content.midiAccessRequestPromise;
      } catch (e) {
        errorMessage = e.name;
      }

      delete content.midiAccessRequestPromise;
      return errorMessage;
    }
  );
  is(rejectionMessage, "SecurityError", "requestMIDIAccess was rejected");

  info("Request midi access again");
  rejectionMessage = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      let errorMessage;
      try {
        await content.wrappedJSObject.navigator.requestMIDIAccess({
          sysex: true,
        });
      } catch (e) {
        errorMessage = e.name;
      }
      return errorMessage;
    }
  );
  is(
    rejectionMessage,
    "SecurityError",
    "requestMIDIAccess was rejected without user prompt"
  );
});

add_task(async function testIframeRequestMIDIAccess() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    PAGE_WITH_IFRAMES_URL
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  info("Check that midi-sysex isn't set");
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.UNKNOWN_ACTION,
      { url: PAGE_WITH_IFRAMES_URL }
    ),
    "midi-sysex value should have UNKNOWN permission"
  );

  info("Request midi access from the same-origin iframe");
  const sameOriginIframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.document.getElementById("sameOrigin").browsingContext;
    }
  );

  let onAddonInstallBlockedNotification = waitForNotification(
    "addon-install-blocked"
  );
  await SpecialPowers.spawn(sameOriginIframeBrowsingContext, [], () => {
    content.midiAccessRequestPromise = content.wrappedJSObject.navigator.requestMIDIAccess(
      {
        sysex: true,
      }
    );
  });

  info("Accept site permission addon install");
  const addonInstallPanel = await onAddonInstallBlockedNotification;
  const notification = addonInstallPanel.childNodes[0];
  const dialogPromise = waitForInstallDialog();
  notification.button.click();
  let installDialog = await dialogPromise;
  installDialog.button.click();

  info("Wait for the midi access request promise to resolve");
  const accessGranted = await SpecialPowers.spawn(
    sameOriginIframeBrowsingContext,
    [],
    async () => {
      try {
        await content.midiAccessRequestPromise;
        return true;
      } catch (e) {}

      delete content.midiAccessRequestPromise;
      return false;
    }
  );
  ok(accessGranted, "requestMIDIAccess resolved");

  info("Check that midi-sysex is now set");
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.ALLOW_ACTION,
      { url: PAGE_WITH_IFRAMES_URL }
    ),
    "midi-sysex value should have ALLOW permission"
  );

  info(
    "Check that we don't prompt user again once they installed the addon from the same-origin iframe"
  );
  const accessPromiseState = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.wrappedJSObject.navigator
        .requestMIDIAccess()
        .then(() => "resolved");
    }
  );
  is(
    accessPromiseState,
    "resolved",
    "requestMIDIAccess resolved without user prompt"
  );

  info("Check that request is rejected when done from a cross-origin iframe");
  const crossOriginIframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.document.getElementById("crossOrigin").browsingContext;
    }
  );

  const rejectionMessage = await SpecialPowers.spawn(
    crossOriginIframeBrowsingContext,
    [],
    async () => {
      let errorName;
      try {
        await content.wrappedJSObject.navigator.requestMIDIAccess({
          sysex: true,
        });
      } catch (e) {
        errorName = e.name;
      }
      return errorName;
    }
  );

  is(
    rejectionMessage,
    "SecurityError",
    "requestMIDIAccess from the remote iframe was rejected"
  );
});

add_task(async function testRequestMIDIAccessLocalhost() {
  const httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerPathHandler(`/test`, function(request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`
      <!DOCTYPE html>
      <meta charset=utf8>
      <h1>Test requestMIDIAccess on lcoalhost</h1>`);
  });
  const localHostTestUrl = `http://localhost:${httpServer.identity.primaryPort}/test`;

  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => httpServer.stop(resolve));
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, localHostTestUrl);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  info("Check that midi-sysex isn't set");
  ok(
    await SpecialPowers.testPermission(
      "midi-sysex",
      SpecialPowers.Services.perms.UNKNOWN_ACTION,
      { url: localHostTestUrl }
    ),
    "midi-sysex value should have UNKNOWN permission"
  );

  info(
    "Request midi access should not prompt for addon install on locahost, but for permission"
  );
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.midiAccessRequestPromise = content.navigator.requestMIDIAccess();
  });
  await popupShown;
  is(
    PopupNotifications.panel.querySelector("popupnotification").id,
    "midi-notification",
    "midi notification was displayed"
  );

  info("Accept permission");
  PopupNotifications.panel
    .querySelector(".popup-notification-primary-button")
    .click();

  info("Wait for the midi access request promise to resolve");
  const accessGranted = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      try {
        await content.midiAccessRequestPromise;
        return true;
      } catch (e) {}

      delete content.midiAccessRequestPromise;
      return false;
    }
  );
  ok(accessGranted, "requestMIDIAccess resolved");

  info("Check that we prompt user again even if they accepted before");
  popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    content.navigator.requestMIDIAccess();
  });
  await popupShown;
  is(
    PopupNotifications.panel.querySelector("popupnotification").id,
    "midi-notification",
    "midi notification was displayed again"
  );
});

add_task(async function testDisabledRequestMIDIAccessFile() {
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("blank.html");
  const fileSchemeTestUri = Services.io.newFileURI(dir).spec;

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, fileSchemeTestUri);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  info("Check that requestMIDIAccess isn't set on navigator on file scheme");
  const isRequestMIDIAccessDefined = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return "requestMIDIAccess" in content.wrappedJSObject.navigator;
    }
  );
  is(
    isRequestMIDIAccessDefined,
    false,
    "navigator.requestMIDIAccess is not defined on file scheme"
  );
});

async function waitForInstallDialog(id = "addon-webext-permissions") {
  let panel = await waitForNotification(id);
  return panel.childNodes[0];
}

const PROGRESS_NOTIFICATION = "addon-progress";
async function waitForNotification(notificationId) {
  info(`Waiting for ${notificationId} notification`);

  let topic = getObserverTopic(notificationId);

  let observerPromise;
  if (notificationId !== "addon-webext-permissions") {
    observerPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        // Ignore the progress notification unless that is the notification we want
        if (
          notificationId != PROGRESS_NOTIFICATION &&
          aTopic == getObserverTopic(PROGRESS_NOTIFICATION)
        ) {
          return;
        }
        Services.obs.removeObserver(observer, topic);
        resolve();
      }, topic);
    });
  }

  let panelEventPromise = new Promise(resolve => {
    window.PopupNotifications.panel.addEventListener(
      "PanelUpdated",
      function eventListener(e) {
        // Skip notifications that are not the one that we are supposed to be looking for
        if (!e.detail.includes(notificationId)) {
          return;
        }
        window.PopupNotifications.panel.removeEventListener(
          "PanelUpdated",
          eventListener
        );
        resolve();
      }
    );
  });

  await observerPromise;
  await panelEventPromise;
  await waitForTick();

  info(`Saw a ${notificationId} notification`);
  await SimpleTest.promiseFocus(window.PopupNotifications.window);
  return window.PopupNotifications.panel;
}

function getObserverTopic(aNotificationId) {
  let topic = aNotificationId;
  if (topic == "xpinstall-disabled") {
    topic = "addon-install-disabled";
  } else if (topic == "addon-progress") {
    topic = "addon-install-started";
  } else if (topic == "addon-install-restart") {
    topic = "addon-install-complete";
  } else if (topic == "addon-installed") {
    topic = "webextension-install-notify";
  }
  return topic;
}

function waitForTick() {
  return new Promise(resolve => executeSoon(resolve));
}
