/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Web Console limits the number of lines displayed according to
// the limit set for each category.

"use strict";

const INIT_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 644419: Console should " +
                 "have user-settable log limits for each message category";

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/test-bug-644419-log-limits.html";

let hud, outputNode;

let test = asyncTest(function* () {
  let { browser } = yield loadTab(INIT_URI);

  hud = yield openConsole();

  hud.jsterm.clearOutput();
  outputNode = hud.outputNode;

  let loaded = loadBrowser(browser);

  expectUncaughtException();

  content.location = TEST_URI;
  yield loaded;

  yield testWebDevLimits();
  yield testWebDevLimits2();
  yield testJsLimits();
  yield testJsLimits2();

  yield testNetLimits();
  yield loadImage();
  yield testCssLimits();
  yield testCssLimits2();

  hud = outputNode = null;
});

function testWebDevLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.console", 10);

  // Find the sentinel entry.
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bar is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });
}

function testWebDevLimits2() {
  // Fill the log with Web Developer errors.
  for (let i = 0; i < 11; i++) {
    content.console.log("test message " + i);
  }

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 10",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    testLogEntry(outputNode, "test message 0", "first message is pruned",
                 false, true);
    findLogEntry("test message 1");
    // Check if the sentinel entry is still there.
    findLogEntry("bar is not defined");

    Services.prefs.clearUserPref("devtools.hud.loglimit.console");
  });
}

function testJsLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.exception", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing JS limits");

  // Find the sentinel entry.
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing JS limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
}

function testJsLimits2() {
  // Fill the log with JS errors.
  let head = content.document.getElementsByTagName("head")[0];
  for (let i = 0; i < 11; i++) {
    let script = content.document.createElement("script");
    script.text = "fubar" + i + ".bogus(6);";

    expectUncaughtException();
    head.insertBefore(script, head.firstChild);
  }

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fubar10 is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  }).then(() => {
    testLogEntry(outputNode, "fubar0 is not defined", "first message is pruned",
                 false, true);
    findLogEntry("fubar1 is not defined");
    // Check if the sentinel entry is still there.
    findLogEntry("testing JS limits");

    Services.prefs.clearUserPref("devtools.hud.loglimit.exception");
  });
}

let gCounter, gImage;

function testNetLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.network", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing Net limits");

  // Find the sentinel entry.
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing Net limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    // Fill the log with network messages.
    gCounter = 0;
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
    return true;
  }

  is(gCounter, 11, "loaded 11 files");

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test-image.png",
      url: "test-image.png?_fubar=10",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    let msgs = outputNode.querySelectorAll(".message[category=network]");
    is(msgs.length, 10, "number of network messages");
    isnot(msgs[0].url.indexOf("fubar=1"), -1, "first network message");
    isnot(msgs[1].url.indexOf("fubar=2"), -1, "second network message");
    findLogEntry("testing Net limits");

    Services.prefs.clearUserPref("devtools.hud.loglimit.network");
  });
}

function testCssLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", 10);

  hud.jsterm.clearOutput();
  content.console.log("testing CSS limits");

  // Find the sentinel entry.
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing CSS limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
}

function testCssLimits2() {
  // Fill the log with CSS errors.
  let body = content.document.getElementsByTagName("body")[0];
  for (let i = 0; i < 11; i++) {
    let div = content.document.createElement("div");
    div.setAttribute("style", "-moz-foobar" + i + ": 42;");
    body.insertBefore(div, body.firstChild);
  }

  return waitForMessages({
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
  });
}
