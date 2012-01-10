/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Web Console limits the number of lines displayed according to
// the limit set for each category.

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/test-bug-644419-log-limits.html";

var gOldPref, gHudId;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab("data:text/html,Web Console test for bug 644419: Console should " +
         "have user-settable log limits for each message category");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  gHudId = HUDService.getHudIdByWindow(content);
  browser.addEventListener("load", testWebDevLimits, true);
  expectUncaughtException();
  content.location = TEST_URI;
}

function testWebDevLimits(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.console");
  Services.prefs.setIntPref("devtools.hud.loglimit.console", 10);

  let hud = HUDService.hudReferences[gHudId];
  outputNode = hud.outputNode;

  executeSoon(function() {
    // Find the sentinel entry.
    findLogEntry("bar is not defined");

    // Fill the log with Web Developer errors.
    for (let i = 0; i < 11; i++) {
      hud.console.log("test message " + i);
    }
    testLogEntry(outputNode, "test message 0", "first message is pruned", false, true);
    findLogEntry("test message 1");
    // Check if the sentinel entry is still there.
    findLogEntry("bar is not defined");

    Services.prefs.setIntPref("devtools.hud.loglimit.console", gOldPref);
    testJsLimits();
  });
}

function testJsLimits(aEvent) {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.exception");
  Services.prefs.setIntPref("devtools.hud.loglimit.exception", 10);

  let hud = HUDService.hudReferences[gHudId];
  hud.jsterm.clearOutput();
  outputNode = hud.outputNode;
  hud.console.log("testing JS limits");

  // Find the sentinel entry.
  findLogEntry("testing JS limits");
  // Fill the log with JS errors.
  let head = content.document.getElementsByTagName("head")[0];
  for (let i = 0; i < 11; i++) {
    var script = content.document.createElement("script");
    script.text = "fubar" + i + ".bogus(6);";
    expectUncaughtException();
    head.insertBefore(script, head.firstChild);
  }

  executeSoon(function() {
    testLogEntry(outputNode, "fubar0 is not defined", "first message is pruned", false, true);
    findLogEntry("fubar1 is not defined");
    // Check if the sentinel entry is still there.
    findLogEntry("testing JS limits");

    Services.prefs.setIntPref("devtools.hud.loglimit.exception", gOldPref);
    testNetLimits();
  });
}

var gCounter, gImage;

function testNetLimits(aEvent) {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.network");
  Services.prefs.setIntPref("devtools.hud.loglimit.network", 10);

  let hud = HUDService.hudReferences[gHudId];
  hud.jsterm.clearOutput();
  outputNode = hud.outputNode;
  hud.console.log("testing Net limits");

  // Find the sentinel entry.
  findLogEntry("testing Net limits");
  // Fill the log with network messages.
  gCounter = 0;
  loadImage();
}

function loadImage() {
  if (gCounter < 11) {
    let body = content.document.getElementsByTagName("body")[0];
    gImage && gImage.removeEventListener("load", loadImage, true);
    gImage = content.document.createElement("img");
    gImage.src = "test-image.png?_fubar=" + gCounter;
    body.insertBefore(gImage, body.firstChild);
    gImage.addEventListener("load", loadImage, true);
    gCounter++;
    return;
  }
  is(gCounter, 11, "loaded 11 files");
  testLogEntry(outputNode, "test-image.png?_fubar=0", "first message is pruned", false, true);
  findLogEntry("test-image.png?_fubar=1");
  // Check if the sentinel entry is still there.
  findLogEntry("testing Net limits");

  Services.prefs.setIntPref("devtools.hud.loglimit.network", gOldPref);
  testCssLimits();
}

function testCssLimits(aEvent) {
  gOldPref = Services.prefs.getIntPref("devtools.hud.loglimit.cssparser");
  Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", 10);

  let hud = HUDService.hudReferences[gHudId];
  hud.jsterm.clearOutput();
  outputNode = hud.outputNode;
  hud.console.log("testing CSS limits");

  // Find the sentinel entry.
  findLogEntry("testing CSS limits");

  // Fill the log with CSS errors.
  let body = content.document.getElementsByTagName("body")[0];
  for (let i = 0; i < 11; i++) {
    var div = content.document.createElement("div");
    div.setAttribute("style", "-moz-foobar" + i + ": 42;");
    body.insertBefore(div, body.firstChild);
  }
  executeSoon(function() {
    testLogEntry(outputNode, "Unknown property '-moz-foobar0'", "first message is pruned", false, true);
    findLogEntry("Unknown property '-moz-foobar1'");
    // Check if the sentinel entry is still there.
    findLogEntry("testing CSS limits");

    Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", gOldPref);
    finishTest();
  });
}
