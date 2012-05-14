/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Console API implements the time() and timeEnd() methods.

function test() {
  addTab("http://example.com/browser/browser/devtools/webconsole/" +
         "test/test-bug-658368-time-methods.html");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  outputNode = hud.outputNode;

  executeSoon(function() {
    findLogEntry("aTimer: timer started");
    findLogEntry("ms");

    // The next test makes sure that timers with the same name but in separate
    // tabs, do not contain the same value.
    addTab("data:text/html;charset=utf-8,<script type='text/javascript'>" +
           "console.timeEnd('bTimer');</script>");
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      openConsole(null, testTimerIndependenceInTabs);
    }, true);
  });
}

function testTimerIndependenceInTabs(hud) {
  outputNode = hud.outputNode;

  executeSoon(function() {
    testLogEntry(outputNode, "bTimer: timer started", "bTimer was not started",
                 false, true);

    // The next test makes sure that timers with the same name but in separate
    // pages, do not contain the same value.
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      executeSoon(testTimerIndependenceInSameTab);
    }, true);
    content.location = "data:text/html;charset=utf-8,<script type='text/javascript'>" +
           "console.time('bTimer');</script>";
  });
}

function testTimerIndependenceInSameTab() {
  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    findLogEntry("bTimer: timer started");
    hud.jsterm.clearOutput();

    // Now the following console.timeEnd() call shouldn't display anything,
    // if the timers in different pages are not related.
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      executeSoon(testTimerIndependenceInSameTabAgain);
    }, true);
    content.location = "data:text/html;charset=utf-8,<script type='text/javascript'>" +
           "console.timeEnd('bTimer');</script>";
  });
}

function testTimerIndependenceInSameTabAgain(hud) {
  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    testLogEntry(outputNode, "bTimer: timer started", "bTimer was not started",
                 false, true);

    closeConsole(gBrowser.selectedTab, function() {
      gBrowser.removeCurrentTab();
      executeSoon(finishTest);
    });
  });
}
