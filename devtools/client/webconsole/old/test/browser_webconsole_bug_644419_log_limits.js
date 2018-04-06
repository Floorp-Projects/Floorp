/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console limits the number of lines displayed according to
// the limit set for each category.

"use strict";

const INIT_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 644419: Console should " +
                 "have user-settable log limits for each message category";

const TEST_URI = "http://example.com/browser/devtools/client/" +
                 "webconsole/old/test/test-bug-644419-log-limits.html";

var hud, outputNode;

add_task(function* () {
  let { browser } = yield loadTab(INIT_URI);

  hud = yield openConsole();

  hud.jsterm.clearOutput();
  outputNode = hud.outputNode;

  let loaded = loadBrowser(browser);

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
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

async function testWebDevLimits2() {
  // Fill the log with Web Developer errors.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    for (let i = 0; i < 11; i++) {
      content.console.log("test message " + i);
    }
  });

  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 10",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  testLogEntry(outputNode, "test message 0", "first message is pruned",
               false, true);
  findLogEntry("test message 1");
  // Check if the sentinel entry is still there.
  findLogEntry("bar is not defined");

  Services.prefs.clearUserPref("devtools.hud.loglimit.console");
}

async function testJsLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.exception", 10);

  hud.jsterm.clearOutput();
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.console.log("testing JS limits");
  });

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

async function testJsLimits2() {
  // Fill the log with JS errors.
  for (let i = 0; i < 11; i++) {
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }
    await ContentTask.spawn(gBrowser.selectedBrowser, i, function(idx) {
      let head = content.document.getElementsByTagName("head")[0];
      let script = content.document.createElement("script");
      script.text = "fubar" + idx + ".bogus(6);";
      head.insertBefore(script, head.firstChild);
    });
  }

  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fubar10 is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });

  testLogEntry(outputNode, "fubar0 is not defined", "first message is pruned",
               false, true);
  findLogEntry("fubar1 is not defined");
  // Check if the sentinel entry is still there.
  findLogEntry("testing JS limits");

  Services.prefs.clearUserPref("devtools.hud.loglimit.exception");
}

var gCounter;

async function testNetLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.network", 10);

  hud.jsterm.clearOutput();
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.console.log("testing Net limits");
  });

  // Find the sentinel entry.
  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testing Net limits",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  // Fill the log with network messages.
  gCounter = 0;
}

async function loadImage() {
  if (gCounter < 11) {
    await ContentTask.spawn(gBrowser.selectedBrowser, gCounter, function(counter) {
      let body = content.document.getElementsByTagName("body")[0];
      let gImage = content.document.createElement("img");
      gImage.src = "test-image.png?_fubar=" + counter;
      body.insertBefore(gImage, body.firstChild);
      return new Promise(function(resolve) {
        gImage.addEventListener("load", resolve, {once: true});
      });
    });
    gCounter++;
    loadImage();
    return true;
  }

  is(gCounter, 11, "loaded 11 files");

  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test-image.png",
      url: "test-image.png?_fubar=10",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });

  let msgs = outputNode.querySelectorAll(".message[category=network]");
  is(msgs.length, 10, "number of network messages");
  isnot(msgs[0].url.indexOf("fubar=1"), -1, "first network message");
  isnot(msgs[1].url.indexOf("fubar=2"), -1, "second network message");
  findLogEntry("testing Net limits");

  Services.prefs.clearUserPref("devtools.hud.loglimit.network");
}

async function testCssLimits() {
  Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", 10);

  hud.jsterm.clearOutput();
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.console.log("testing CSS limits");
  });

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

async function testCssLimits2() {
  // Fill the log with CSS errors.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let body = content.document.getElementsByTagName("body")[0];
    for (let i = 0; i < 11; i++) {
      let div = content.document.createElement("div");
      div.setAttribute("style", "-moz-foobar" + i + ": 42;");
      body.insertBefore(div, body.firstChild);
    }
  });

  await waitForMessages({
    webconsole: hud,
    messages: [{
      text: "-moz-foobar10",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    }],
  });

  testLogEntry(outputNode, "Unknown property \u2018-moz-foobar0\u2019",
               "first message is pruned", false, true);
  findLogEntry("Unknown property \u2018-moz-foobar1\u2019");
  // Check if the sentinel entry is still there.
  findLogEntry("testing CSS limits");

  Services.prefs.clearUserPref("devtools.hud.loglimit.cssparser");
}
