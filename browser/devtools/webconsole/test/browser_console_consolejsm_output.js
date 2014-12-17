/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that Console.jsm outputs messages to the Browser Console, bug 851231.

"use strict";

let test = asyncTest(function*() {
  let storage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  let console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;
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
});
