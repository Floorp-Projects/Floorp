/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar errors count works properly.

function test() {
  const TEST_URI = "http://example.com/browser/browser/devtools/shared/test/" +
                   "browser_toolbar_webconsole_errors_count.html";

  let gDevTools = Cu.import("resource:///modules/devtools/gDevTools.jsm",
                             {}).gDevTools;

  let webconsole = document.getElementById("developer-toolbar-toolbox-button");
  let tab1, tab2;

  Services.prefs.setBoolPref("javascript.options.strict", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("javascript.options.strict");
  });

  ignoreAllUncaughtExceptions();
  addTab(TEST_URI, openToolbar);

  function openToolbar(browser, tab) {
    tab1 = tab;
    ignoreAllUncaughtExceptions(false);

    expectUncaughtException();

    if (!DeveloperToolbar.visible) {
      DeveloperToolbar.show(true, onOpenToolbar);
    }
    else {
      onOpenToolbar();
    }
  }

  function onOpenToolbar() {
    ok(DeveloperToolbar.visible, "DeveloperToolbar is visible");

    waitForButtonUpdate({
      name: "web console button shows page errors",
      errors: 3,
      warnings: 0,
      callback: addErrors,
    });
  }

  function addErrors() {
    expectUncaughtException();

    waitForFocus(function() {
      let button = content.document.querySelector("button");
      executeSoon(function() {
        EventUtils.synthesizeMouse(button, 3, 2, {}, content);
      });
    }, content);

    waitForButtonUpdate({
      name: "button shows one more error after click in page",
      errors: 4,
      warnings: 1,
      callback: () => {
        ignoreAllUncaughtExceptions();
        addTab(TEST_URI, onOpenSecondTab);
      },
    });
  }

  function onOpenSecondTab(browser, tab) {
    tab2 = tab;

    ignoreAllUncaughtExceptions(false);
    expectUncaughtException();

    waitForButtonUpdate({
      name: "button shows correct number of errors after new tab is open",
      errors: 3,
      warnings: 0,
      callback: switchToTab1,
    });
  }

  function switchToTab1() {
    gBrowser.selectedTab = tab1;
    waitForButtonUpdate({
      name: "button shows the page errors from tab 1",
      errors: 4,
      warnings: 1,
      callback: openWebConsole.bind(null, tab1, onWebConsoleOpen),
    });
  }

  function onWebConsoleOpen(hud) {
    dump("lolz!!\n");
    waitForValue({
      name: "web console shows the page errors",
      validator: function() {
        return hud.outputNode.querySelectorAll(".message[category=exception][severity=error]").length;
      },
      value: 4,
      success: checkConsoleOutput.bind(null, hud),
      failure: () => {
        finish();
      },
    });
  }

  function checkConsoleOutput(hud) {
    let msgs = ["foobarBug762996a", "foobarBug762996b", "foobarBug762996load",
                "foobarBug762996click", "foobarBug762996consoleLog",
                "foobarBug762996css", "fooBug788445"];
    msgs.forEach(function(msg) {
      isnot(hud.outputNode.textContent.indexOf(msg), -1,
            msg + " found in the Web Console output");
    });

    hud.jsterm.clearOutput();

    is(hud.outputNode.textContent.indexOf("foobarBug762996color"), -1,
       "clearOutput() worked");

    expectUncaughtException();
    let button = content.document.querySelector("button");
    EventUtils.synthesizeMouse(button, 2, 2, {}, content);

    waitForButtonUpdate({
      name: "button shows one more error after another click in page",
      errors: 5,
      warnings: 1, // warnings are not repeated by the js engine
      callback: () => waitForValue(waitForNewError),
    });

    let waitForNewError = {
      name: "the Web Console displays the new error",
      validator: function() {
        return hud.outputNode.textContent.indexOf("foobarBug762996click") > -1;
      },
      success: doClearConsoleButton.bind(null, hud),
      failure: finish,
    };
  }

  function doClearConsoleButton(hud) {
    let clearButton = hud.ui.rootElement
                      .querySelector(".webconsole-clear-console-button");
    EventUtils.synthesizeMouse(clearButton, 2, 2, {}, hud.iframeWindow);

    is(hud.outputNode.textContent.indexOf("foobarBug762996click"), -1,
       "clear console button worked");
    is(getErrorsCount(), 0, "page errors counter has been reset");
    let tooltip = getTooltipValues();
    is(tooltip[1], 0, "page warnings counter has been reset");

    doPageReload(hud);
  }

  function doPageReload(hud) {
    tab1.linkedBrowser.addEventListener("load", onReload, true);

    ignoreAllUncaughtExceptions();
    content.location.reload();

    function onReload() {
      tab1.linkedBrowser.removeEventListener("load", onReload, true);
      ignoreAllUncaughtExceptions(false);
      expectUncaughtException();

      waitForButtonUpdate({
        name: "the Web Console button count has been reset after page reload",
        errors: 3,
        warnings: 0,
        callback: waitForValue.bind(null, waitForConsoleOutputAfterReload),
      });
    }

    let waitForConsoleOutputAfterReload = {
      name: "the Web Console displays the correct number of errors after reload",
      validator: function() {
        return hud.outputNode.querySelectorAll(".message[category=exception][severity=error]").length;
      },
      value: 3,
      success: function() {
        isnot(hud.outputNode.textContent.indexOf("foobarBug762996load"), -1,
              "foobarBug762996load found in console output after page reload");
        testEnd();
      },
      failure: testEnd,
    };
  }

  function testEnd() {
    document.getElementById("developer-toolbar-closebutton").doCommand();
    let target1 = TargetFactory.forTab(tab1);
    gDevTools.closeToolbox(target1);
    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    finish();
  }

  // Utility functions

  function getErrorsCount() {
    let count = webconsole.getAttribute("error-count");
    return count ? count : "0";
  }

  function getTooltipValues() {
    let matches = webconsole.getAttribute("tooltiptext")
                  .match(/(\d+) errors?, (\d+) warnings?/);
    return matches ? [matches[1], matches[2]] : [0, 0];
  }

  function waitForButtonUpdate(options) {
    function check() {
      let errors = getErrorsCount();
      let tooltip = getTooltipValues();
      let result = errors == options.errors && tooltip[1] == options.warnings;
      if (result) {
        ok(true, options.name);
        is(errors, tooltip[0], "button error-count is the same as in the tooltip");

        // Get out of the toolbar event execution loop.
        executeSoon(options.callback);
      }
      return result;
    }

    if (!check()) {
      info("wait for: " + options.name);
      DeveloperToolbar.on("errors-counter-updated", function onUpdate(event) {
        if (check()) {
          DeveloperToolbar.off(event, onUpdate);
        }
      });
    }
  }

  function openWebConsole(tab, callback)
  {
    let target = TargetFactory.forTab(tab);
    gDevTools.showToolbox(target, "webconsole").then((toolbox) =>
      callback(toolbox.getCurrentPanel().hud));
  }
}
