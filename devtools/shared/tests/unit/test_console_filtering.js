/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { console, ConsoleAPI } = require("resource://gre/modules/Console.jsm");
const { ConsoleAPIListener } = require("devtools/server/actors/utils/webconsole-listeners");
const Services = require("Services");

var seenMessages = 0;
var seenTypes = 0;

var callback = {
  onConsoleAPICall: function (message) {
    if (message.consoleID && message.consoleID == "addon/foo") {
      do_check_eq(message.level, "warn");
      do_check_eq(message.arguments[0], "Warning from foo");
      seenTypes |= 1;
    } else if (message.addonId == "bar") {
      do_check_eq(message.level, "error");
      do_check_eq(message.arguments[0], "Error from bar");
      seenTypes |= 2;
    } else {
      do_check_eq(message.level, "log");
      do_check_eq(message.arguments[0], "Hello from default console");
      seenTypes |= 4;
    }
    seenMessages++;
  }
};

let policy;
do_register_cleanup(() => {
  policy.active = false;
});

function createFakeAddonWindow({addonId} = {}) {
  const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  const uuid = uuidGen.generateUUID().number.slice(1, -1);

  if (policy) {
    policy.active = false;
  }
  /* globals MatchPatternSet, WebExtensionPolicy */
  policy = new WebExtensionPolicy({
    id: addonId,
    mozExtensionHostname: uuid,
    baseURL: "file:///",
    allowedOrigins: new MatchPatternSet([]),
    localizeCallback() {},
  });
  policy.active = true;

  let baseURI = Services.io.newURI(`moz-extension://${uuid}/`);
  let principal = Services.scriptSecurityManager
        .createCodebasePrincipal(baseURI, {});
  let chromeWebNav = Services.appShell.createWindowlessBrowser(true);
  let docShell = chromeWebNav.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDocShell);
  docShell.createAboutBlankContentViewer(principal);
  let addonWindow = docShell.contentViewer.DOMDocument.defaultView;

  return {addonWindow, chromeWebNav};
}

/**
 * Tests that the consoleID property of the ConsoleAPI options gets passed
 * through to console messages.
 */
function run_test() {
  // console1 Test Console.jsm messages tagged by the Addon SDK
  // are still filtered correctly.
  let console1 = new ConsoleAPI({
    consoleID: "addon/foo"
  });

  // console2 - WebExtension page's console messages tagged
  // by 'originAttributes.addonId' are filtered correctly.
  let {addonWindow, chromeWebNav} = createFakeAddonWindow({addonId: "bar"});
  let console2 = addonWindow.console;

  // console - Plain console object (messages are tagged with window ids
  // and originAttributes, but the addonId will be empty).
  console.log("Hello from default console");

  console1.warn("Warning from foo");
  console2.error("Error from bar");

  let listener = new ConsoleAPIListener(null, callback);
  listener.init();
  let messages = listener.getCachedMessages();

  seenTypes = 0;
  seenMessages = 0;
  messages.forEach(callback.onConsoleAPICall);
  do_check_eq(seenMessages, 3);
  do_check_eq(seenTypes, 7);

  seenTypes = 0;
  seenMessages = 0;
  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");
  do_check_eq(seenMessages, 3);
  do_check_eq(seenTypes, 7);

  listener.destroy();

  listener = new ConsoleAPIListener(null, callback, {addonId: "foo"});
  listener.init();
  messages = listener.getCachedMessages();

  seenTypes = 0;
  seenMessages = 0;
  messages.forEach(callback.onConsoleAPICall);
  do_check_eq(seenMessages, 2);
  do_check_eq(seenTypes, 1);

  seenTypes = 0;
  seenMessages = 0;
  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");
  do_check_eq(seenMessages, 1);
  do_check_eq(seenTypes, 1);

  listener.destroy();

  listener = new ConsoleAPIListener(null, callback, {addonId: "bar"});
  listener.init();
  messages = listener.getCachedMessages();

  seenTypes = 0;
  seenMessages = 0;
  messages.forEach(callback.onConsoleAPICall);
  do_check_eq(seenMessages, 3);
  do_check_eq(seenTypes, 2);

  seenTypes = 0;
  seenMessages = 0;
  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");

  do_check_eq(seenMessages, 1);
  do_check_eq(seenTypes, 2);

  listener.destroy();

  // Close the addon window's chromeWebNav.
  chromeWebNav.close();
}
