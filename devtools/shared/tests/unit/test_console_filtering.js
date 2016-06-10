/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { console, ConsoleAPI } = require("resource://gre/modules/Console.jsm");
const { ConsoleAPIListener } = require("devtools/shared/webconsole/utils");
const Services = require("Services");

var seenMessages = 0;
var seenTypes = 0;

var callback = {
  onConsoleAPICall: function (aMessage) {
    if (aMessage.consoleID && aMessage.consoleID == "addon/foo") {
      do_check_eq(aMessage.level, "warn");
      do_check_eq(aMessage.arguments[0], "Warning from foo");
      seenTypes |= 1;
    } else if (aMessage.originAttributes &&
              aMessage.originAttributes.addonId == "bar") {
      do_check_eq(aMessage.level, "error");
      do_check_eq(aMessage.arguments[0], "Error from bar");
      seenTypes |= 2;
    } else {
      do_check_eq(aMessage.level, "log");
      do_check_eq(aMessage.arguments[0], "Hello from default console");
      seenTypes |= 4;
    }
    seenMessages++;
  }
};

function createFakeAddonWindow({addonId} = {}) {
  let baseURI = Services.io.newURI("about:blank", null, null);
  let originAttributes = {addonId};
  let principal = Services.scriptSecurityManager
        .createCodebasePrincipal(baseURI, originAttributes);
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
