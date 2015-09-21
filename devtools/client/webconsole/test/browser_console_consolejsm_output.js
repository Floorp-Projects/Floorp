/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that Console.jsm outputs messages to the Browser Console, bug 851231.

"use strict";

function onNewMessage(aEvent, aNewMessages) {
  for (let msg of aNewMessages) {
    // Messages that shouldn't be output contain the substring FAIL_TEST
    if (msg.node.textContent.includes("FAIL_TEST")) {
      ok(false, "Message shouldn't have been output: " + msg.node.textContent);
    }
  }
}

add_task(function*() {
  let consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  let storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  let {console} = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
  console.log("bug861338-log-cached");

  let hud = yield HUDService.toggleBrowserConsole();

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "cached console.log message",
      text: "bug861338-log-cached",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  hud.jsterm.clearOutput(true);

  function testTrace() {
    console.trace();
  }

  console.time("foobarTimer");
  let foobar = { bug851231prop: "bug851231value" };

  console.log("bug851231-log");
  console.info("bug851231-info");
  console.warn("bug851231-warn");
  console.error("bug851231-error", foobar);
  console.debug("bug851231-debug");
  console.dir(document);
  testTrace();
  console.timeEnd("foobarTimer");

  info("wait for the Console.jsm messages");

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "console.log output",
        text: "bug851231-log",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "console.info output",
        text: "bug851231-info",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_INFO,
      },
      {
        name: "console.warn output",
        text: "bug851231-warn",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_WARNING,
      },
      {
        name: "console.error output",
        text: /\bbug851231-error\b.+\{\s*bug851231prop:\s"bug851231value"\s*\}/,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        objects: true,
      },
      {
        name: "console.debug output",
        text: "bug851231-debug",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "console.trace output",
        consoleTrace: {
          file: "browser_console_consolejsm_output.js",
          fn: "testTrace",
        },
      },
      {
        name: "console.dir output",
        consoleDir: /XULDocument\s+.+\s+chrome:\/\/.+\/browser\.xul/,
      },
      {
        name: "console.time output",
        consoleTime: "foobarTimer",
      },
      {
        name: "console.timeEnd output",
        consoleTimeEnd: "foobarTimer",
      },
    ],
  });

  let consoleErrorMsg = results[3];
  ok(consoleErrorMsg, "console.error message element found");
  let clickable = consoleErrorMsg.clickableElements[0];
  ok(clickable, "clickable object found for console.error");

  let deferred = promise.defer();

  let onFetch = (aEvent, aVar) => {
    // Skip the notification from console.dir variablesview-fetched.
    if (aVar._variablesView != hud.jsterm._variablesView) {
      return;
    }
    hud.jsterm.off("variablesview-fetched", onFetch);

    deferred.resolve(aVar);
  };

  hud.jsterm.on("variablesview-fetched", onFetch);

  clickable.scrollIntoView(false);

  info("wait for variablesview-fetched");
  executeSoon(() =>
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow));

  let varView = yield deferred.promise;
  ok(varView, "object inspector opened on click");

  yield findVariableViewProperties(varView, [{
    name: "bug851231prop",
    value: "bug851231value",
  }], { webconsole: hud });

  yield HUDService.toggleBrowserConsole();
});

add_task(function* testPrefix() {
  let consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  let storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  let {ConsoleAPI} = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
  let consoleOptions = {
    maxLogLevel: "error",
    prefix: "Log Prefix",
  };
  let console2 = new ConsoleAPI(consoleOptions);
  console2.error("Testing a prefix");
  console2.log("FAIL_TEST: Below the maxLogLevel");

  let hud = yield HUDService.toggleBrowserConsole();
  hud.ui.on("new-messages", onNewMessage);
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "cached console.error message",
      prefix: "Log Prefix:",
      severity: SEVERITY_ERROR,
      text: "Testing a prefix",
    }],
  });

  hud.jsterm.clearOutput(true);
  hud.ui.off("new-messages", onNewMessage);
  yield HUDService.toggleBrowserConsole();
});

add_task(function* testMaxLogLevelPrefMissing() {
  let consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  let storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  let {ConsoleAPI} = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
  let consoleOptions = {
    maxLogLevel: "error",
    maxLogLevelPref: "testing.maxLogLevel",
  };
  let console = new ConsoleAPI(consoleOptions);

  is(Services.prefs.getPrefType(consoleOptions.maxLogLevelPref),
     Services.prefs.PREF_INVALID,
     "Check log level pref is missing");

  // Since the maxLogLevelPref doesn't exist, we should fallback to the passed
  // maxLogLevel of "error".
  console.warn("FAIL_TEST: Below the maxLogLevel");
  console.error("Error should be shown");

  let hud = yield HUDService.toggleBrowserConsole();

  hud.ui.on("new-messages", onNewMessage);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "defaulting to error level",
      severity: SEVERITY_ERROR,
      text: "Error should be shown",
    }],
  });

  hud.jsterm.clearOutput(true);
  hud.ui.off("new-messages", onNewMessage);
  yield HUDService.toggleBrowserConsole();
});

add_task(function* testMaxLogLevelPref() {
  let consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  let storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  let {ConsoleAPI} = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
  let consoleOptions = {
    maxLogLevel: "error",
    maxLogLevelPref: "testing.maxLogLevel",
  };

  info("Setting the pref to warn");
  Services.prefs.setCharPref(consoleOptions.maxLogLevelPref, "Warn");

  let console = new ConsoleAPI(consoleOptions);

  is(console.maxLogLevel, "warn", "Check pref was read at initialization");

  console.info("FAIL_TEST: info is below the maxLogLevel");
  console.error("Error should be shown");
  console.warn("Warn should be shown due to the initial pref value");

  info("Setting the pref to info");
  Services.prefs.setCharPref(consoleOptions.maxLogLevelPref, "INFO");
  is(console.maxLogLevel, "info", "Check pref was lowercased");

  console.info("info should be shown due to the pref change being observed");

  info("Clearing the pref");
  Services.prefs.clearUserPref(consoleOptions.maxLogLevelPref);

  console.warn("FAIL_TEST: Shouldn't be shown due to defaulting to error");
  console.error("Should be shown due to defaulting to error");

  let hud = yield HUDService.toggleBrowserConsole();
  hud.ui.on("new-messages", onNewMessage);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "error > warn",
      severity: SEVERITY_ERROR,
      text: "Error should be shown",
    },
    {
      name: "warn is the inital pref value",
      severity: SEVERITY_WARNING,
      text: "Warn should be shown due to the initial pref value",
    },
    {
      name: "pref changed to info",
      severity: SEVERITY_INFO,
      text: "info should be shown due to the pref change being observed",
    },
    {
      name: "default to intial maxLogLevel if pref is removed",
      severity: SEVERITY_ERROR,
      text: "Should be shown due to defaulting to error",
    }],
  });

  hud.jsterm.clearOutput(true);
  hud.ui.off("new-messages", onNewMessage);
  yield HUDService.toggleBrowserConsole();
});
