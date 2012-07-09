/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar errors count works properly.

function test() {
  const TEST_URI = "http://example.com/browser/browser/devtools/shared/test/browser_toolbar_webconsole_errors_count.html";

  let imported = {};
  Components.utils.import("resource:///modules/HUDService.jsm", imported);
  let HUDService = imported.HUDService;

  let webconsole = document.getElementById("developer-toolbar-webconsole");
  let toolbar = document.getElementById("Tools:DevToolbar");
  let tab1, tab2;

  function openToolbar(browser, tab) {
    tab1 = tab;
    ignoreAllUncaughtExceptions(false);

    ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible");

    expectUncaughtException();
    oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW, onOpenToolbar);
    toolbar.doCommand();
  }

  ignoreAllUncaughtExceptions();
  addTab(TEST_URI, openToolbar);

  function getErrorsCount() {
    let count = webconsole.getAttribute("error-count");
    return count ? count : "0";
  }

  function onOpenToolbar() {
    ok(DeveloperToolbar.visible, "DeveloperToolbar is visible");

    waitForValue({
      name: "web console button shows page errors",
      validator: getErrorsCount,
      value: 3,
      success: addErrors,
      failure: finish,
    });
  }

  function addErrors() {
    expectUncaughtException();
    let button = content.document.querySelector("button");
    EventUtils.synthesizeMouse(button, 2, 2, {}, content);

    waitForValue({
      name: "button shows one more error after click in page",
      validator: getErrorsCount,
      value: 4,
      success: function() {
        ignoreAllUncaughtExceptions();
        addTab(TEST_URI, onOpenSecondTab);
      },
      failure: finish,
    });
  }

  function onOpenSecondTab(browser, tab) {
    tab2 = tab;

    ignoreAllUncaughtExceptions(false);
    expectUncaughtException();

    waitForValue({
      name: "button shows correct number of errors after new tab is open",
      validator: getErrorsCount,
      value: 3,
      success: switchToTab1,
      failure: finish,
    });
  }

  function switchToTab1() {
    gBrowser.selectedTab = tab1;
    waitForValue({
      name: "button shows the page errors from tab 1",
      validator: getErrorsCount,
      value: 4,
      success: function() {
        openWebConsole(tab1, onWebConsoleOpen);
      },
      failure: finish,
    });
  }

  function openWebConsole(tab, callback)
  {
    function _onWebConsoleOpen(subject)
    {
      subject.QueryInterface(Ci.nsISupportsString);
      let hud = HUDService.getHudReferenceById(subject.data);
      executeSoon(callback.bind(null, hud));
    }

    oneTimeObserve("web-console-created", _onWebConsoleOpen);

    HUDService.activateHUDForContext(tab);
  }

  function onWebConsoleOpen(hud) {
    waitForValue({
      name: "web console shows the page errors",
      validator: function() {
        return hud.outputNode.querySelectorAll(".hud-exception").length;
      },
      value: 4,
      success: checkConsoleOutput.bind(null, hud),
      failure: finish,
    });
  }

  function checkConsoleOutput(hud) {
    let errors = ["foobarBug762996a", "foobarBug762996b", "foobarBug762996load",
                  "foobarBug762996click", "foobarBug762996consoleLog",
                  "foobarBug762996css"];
    errors.forEach(function(error) {
      isnot(hud.outputNode.textContent.indexOf(error), -1,
            error + " found in the Web Console output");
    });

    hud.jsterm.clearOutput();

    is(hud.outputNode.textContent.indexOf("foobarBug762996color"), -1,
       "clearOutput() worked");

    expectUncaughtException();
    let button = content.document.querySelector("button");
    EventUtils.synthesizeMouse(button, 2, 2, {}, content);

    waitForValue({
      name: "button shows one more error after another click in page",
      validator: getErrorsCount,
      value: 5,
      success: function() {
        waitForValue(waitForNewError);
      },
      failure: finish,
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
    let clearButton = hud.HUDBox
                      .querySelector(".webconsole-clear-console-button");
    EventUtils.synthesizeMouse(clearButton, 2, 2, {}, window);

    is(hud.outputNode.textContent.indexOf("foobarBug762996click"), -1,
       "clear console button worked");
    is(getErrorsCount(), 0, "page errors counter has been reset");

    doPageReload(hud);
  }

  function doPageReload(hud) {
    tab1.linkedBrowser.addEventListener("load", function _onReload() {
      tab1.linkedBrowser.removeEventListener("load", _onReload, true);
      ignoreAllUncaughtExceptions(false);
      expectUncaughtException();
    }, true);

    ignoreAllUncaughtExceptions();
    content.location.reload();

    waitForValue({
      name: "the Web Console button count has been reset after page reload",
      validator: getErrorsCount,
      value: 3,
      success: function() {
        waitForValue(waitForConsoleOutputAfterReload);
      },
      failure: finish,
    });

    let waitForConsoleOutputAfterReload = {
      name: "the Web Console displays the correct number of errors after reload",
      validator: function() {
        return hud.outputNode.querySelectorAll(".hud-exception").length;
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
    HUDService.deactivateHUDForContext(tab1);
    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    finish();
  }

  function oneTimeObserve(name, callback) {
    function _onObserve(aSubject, aTopic, aData) {
      Services.obs.removeObserver(_onObserve, name);
      callback(aSubject, aTopic, aData);
    };
    Services.obs.addObserver(_onObserve, name, false);
  }
}

