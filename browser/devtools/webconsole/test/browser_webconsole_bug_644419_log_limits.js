/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Web Console limits the number of lines displayed according to
// the limit set for each category.

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/test-bug-644419-log-limits.html";

let hud, outputNode;

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 644419: Console should " +
         "have user-settable log limits for each message category");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, onLoad, true);

  openConsole(null, function(aHud) {
    aHud.jsterm.clearOutput();
    hud = aHud;
    outputNode = aHud.outputNode;

    browser.addEventListener("load", testWebDevLimits, true);
    expectUncaughtException();
    content.location = TEST_URI;
  });
}

function testWebDevLimits(aEvent) {
  browser.removeEventListener(aEvent.type, testWebDevLimits, true);
  Services.prefs.setIntPref("devtools.hud.loglimit.console", 10);

  // Find the sentinel entry.
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bar is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  }).then(testWebDevLimits2);
}

function testWebDevLimits2() {
  // Fill the log with Web Developer errors.
  for (let i = 0; i < 11; i++) {
    content.console.log("test message " + i);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 10",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    testLogEntry(outputNode, "test message 0", "first message is pruned", false, true);
    findLogEntry("test message 1");
    // Check if the sentinel entry is still there.
    findLogEntry("bar is not defined");

    Services.prefs.clearUserPref("devtools.hud.loglimit.console");
    testJsLimits();
  });
}

function testJsLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.exception", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing JS limits");

  // Find the sentinel entry.
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing JS limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testJsLimits2);
}

function testJsLimits2() {
  // Fill the log with JS errors.
  let head = content.document.getElementsByTagName("head")[0];
  for (let i = 0; i < 11; i++) {
    var script = content.document.createElement("script");
    script.text = "fubar" + i + ".bogus(6);";
    expectUncaughtException();
    head.insertBefore(script, head.firstChild);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fubar10 is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  }).then(() => {
    testLogEntry(outputNode, "fubar0 is not defined", "first message is pruned", false, true);
    findLogEntry("fubar1 is not defined");
    // Check if the sentinel entry is still there.
    findLogEntry("testing JS limits");

    Services.prefs.clearUserPref("devtools.hud.loglimit.exception");
    testNetLimits();
  });
}

var gCounter, gImage;

function testNetLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.network", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing Net limits");

  // Find the sentinel entry.
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing Net limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    // Fill the log with network messages.
    gCounter = 0;
    loadImage();
  });
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

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test-image.png?_fubar=10",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    testLogEntry(outputNode, "test-image.png?_fubar=0", "first message is pruned", false, true);
    findLogEntry("test-image.png?_fubar=1");
    // Check if the sentinel entry is still there.
    findLogEntry("testing Net limits");

    Services.prefs.clearUserPref("devtools.hud.loglimit.network");
    testCssLimits();
  });
}

function testCssLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing CSS limits");

  // Find the sentinel entry.
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing CSS limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testCssLimits2);
}

function testCssLimits2() {
  // Fill the log with CSS errors.
  let body = content.document.getElementsByTagName("body")[0];
  for (let i = 0; i < 11; i++) {
    var div = content.document.createElement("div");
    div.setAttribute("style", "-moz-foobar" + i + ": 42;");
    body.insertBefore(div, body.firstChild);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "-moz-foobar10",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    }],
  }).then(() => {
    testLogEntry(outputNode, "Unknown property '-moz-foobar0'",
                 "first message is pruned", false, true);
    findLogEntry("Unknown property '-moz-foobar1'");
    // Check if the sentinel entry is still there.
    findLogEntry("testing CSS limits");

    Services.prefs.clearUserPref("devtools.hud.loglimit.cssparser");
    finishTest();
  });
}
