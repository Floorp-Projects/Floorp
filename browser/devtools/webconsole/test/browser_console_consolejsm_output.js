/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that Console.jsm outputs messages to the Browser Console, bug 851231.

function test()
{
  let storage = Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm", {}).ConsoleAPIStorage;
  storage.clearEvents();

  let console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;
  console.log("bug861338-log-cached");

  HUDService.toggleBrowserConsole().then(consoleOpened);
  let hud = null;

  function consoleOpened(aHud)
  {
    hud = aHud;
    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "cached console.log message",
        text: "bug861338-log-cached",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(onCachedMessage);
  }

  function onCachedMessage()
  {
    hud.jsterm.clearOutput(true);

    console.time("foobarTimer");
    let foobar = { bug851231prop: "bug851231value" };

    console.log("bug851231-log");
    console.info("bug851231-info");
    console.warn("bug851231-warn");
    console.error("bug851231-error", foobar);
    console.debug("bug851231-debug");
    console.trace();
    console.dir(document);
    console.timeEnd("foobarTimer");

    info("wait for the Console.jsm messages");

    waitForMessages({
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
          text: /\bbug851231-error\b.+\[object Object\]/,
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
            fn: "onCachedMessage",
          },
        },
        {
          name: "console.dir output",
          consoleDir: "[object XULDocument]",
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
    }).then((aResults) => {
      let consoleErrorMsg = aResults[3];
      ok(consoleErrorMsg, "console.error message element found");
      let clickable = consoleErrorMsg.clickableElements[0];
      ok(clickable, "clickable object found for console.error");

      let onFetch = (aEvent, aVar) => {
        // Skip the notification from console.dir variablesview-fetched.
        if (aVar._variablesView != hud.jsterm._variablesView) {
          return;
        }
        hud.jsterm.off("variablesview-fetched", onFetch);

        ok(aVar, "object inspector opened on click");

        findVariableViewProperties(aVar, [{
          name: "bug851231prop",
          value: "bug851231value",
        }], { webconsole: hud }).then(finishTest);
      };

      hud.jsterm.on("variablesview-fetched", onFetch);

      scrollOutputToNode(clickable);

      info("wait for variablesview-fetched");
      executeSoon(() =>
        EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow));
    });
  }
}
