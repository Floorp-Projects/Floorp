/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
      .getService(Ci.nsIConsoleAPIStorage);

const {WebExtensionPolicy} = Cu.import("resource://gre/modules/Services.jsm", {});

const FAKE_ADDON_ID = "test-webext-addon@mozilla.org";
const EXPECTED_CONSOLE_ID = `addon/${FAKE_ADDON_ID}`;
const EXPECTED_CONSOLE_MESSAGE_CONTENT = "fake-webext-addon-test-log-message";
const ConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init() {
    Services.obs.addObserver(this, "console-api-log-event");
  },

  uninit() {
    Services.obs.removeObserver(this, "console-api-log-event");
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "console-api-log-event") {
      let consoleAPIMessage = aSubject.wrappedJSObject;

      is(consoleAPIMessage.arguments[0], EXPECTED_CONSOLE_MESSAGE_CONTENT,
         "the consoleAPIMessage contains the expected message");

      is(consoleAPIMessage.addonId, FAKE_ADDON_ID,
         "the consoleAPImessage originAttributes contains the expected addonId");

      let cachedMessages = ConsoleAPIStorage.getEvents().filter((msg) => {
        return msg.addonId == FAKE_ADDON_ID;
      });

      is(cachedMessages.length, 1, "found the expected cached console messages from the addon");
      is(cachedMessages[0] && cachedMessages[0].addonId, FAKE_ADDON_ID,
         "the cached message originAttributes contains the expected addonId");

      finish();
    }
  }
};

function test()
{
  ConsoleObserver.init();

  waitForExplicitFinish();

  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  let uuid = uuidGenerator.generateUUID().number;
  uuid = uuid.slice(1, -1); // Strip { and } off the UUID.

  const url = `moz-extension://${uuid}/`;
  /* globals MatchPatternSet, WebExtensionPolicy */
  let policy = new WebExtensionPolicy({
    id: FAKE_ADDON_ID,
    mozExtensionHostname: uuid,
    baseURL: "file:///",
    allowedOrigins: new MatchPatternSet([]),
    localizeCallback() {},
  });
  policy.active = true;

  let baseURI = Services.io.newURI(url);
  let principal = Services.scriptSecurityManager
        .createCodebasePrincipal(baseURI, {});

  let chromeWebNav = Services.appShell.createWindowlessBrowser(true);
  let interfaceRequestor = chromeWebNav.QueryInterface(Ci.nsIInterfaceRequestor);
  let docShell = interfaceRequestor.getInterface(Ci.nsIDocShell);
  docShell.createAboutBlankContentViewer(principal);

  info("fake webextension docShell created");

  registerCleanupFunction(function() {
    policy.active = false;
    if (chromeWebNav) {
      chromeWebNav.close();
      chromeWebNav = null;
    }
    ConsoleObserver.uninit();
  });

  let window = docShell.contentViewer.DOMDocument.defaultView;
  window.eval(`console.log("${EXPECTED_CONSOLE_MESSAGE_CONTENT}");`);
  chromeWebNav.close();
  chromeWebNav = null;

  info("fake webextension page logged a console api message");
}
