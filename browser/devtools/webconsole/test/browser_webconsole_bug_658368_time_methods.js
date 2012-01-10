/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Console API implements the time() and timeEnd() methods.

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab("http://example.com/browser/browser/devtools/webconsole/" +
         "test/test-bug-658368-time-methods.html");
  openConsole();
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, onLoad, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    findLogEntry("aTimer: timer started");
    findLogEntry("ms");

    // The next test makes sure that timers with the same name but in separate
    // tabs, do not contain the same value.
    addTab("data:text/html,<script type='text/javascript'>" +
           "console.timeEnd('bTimer');</script>");
    openConsole();
    browser.addEventListener("load", testTimerIndependenceInTabs, true);
  });
}

function testTimerIndependenceInTabs(aEvent) {
  browser.removeEventListener(aEvent.type, testTimerIndependenceInTabs, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    testLogEntry(outputNode, "bTimer: timer started", "bTimer was not started",
                 false, true);

    // The next test makes sure that timers with the same name but in separate
    // pages, do not contain the same value.
    browser.addEventListener("load", testTimerIndependenceInSameTab, true);
    content.location = "data:text/html,<script type='text/javascript'>" +
           "console.time('bTimer');</script>";
  });
}

function testTimerIndependenceInSameTab(aEvent) {
  browser.removeEventListener(aEvent.type, testTimerIndependenceInSameTab, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    findLogEntry("bTimer: timer started");
    hud.jsterm.clearOutput();

    // Now the following console.timeEnd() call shouldn't display anything,
    // if the timers in different pages are not related.
    browser.addEventListener("load", testTimerIndependenceInSameTabAgain, true);
    content.location = "data:text/html,<script type='text/javascript'>" +
           "console.timeEnd('bTimer');</script>";
  });
}

function testTimerIndependenceInSameTabAgain(aEvent) {
  browser.removeEventListener(aEvent.type, testTimerIndependenceInSameTabAgain, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    testLogEntry(outputNode, "bTimer: timer started", "bTimer was not started",
                 false, true);

    finishTest();
  });
}
