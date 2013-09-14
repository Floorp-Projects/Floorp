/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure messages are not considered repeated when coming from
// different lines of code, or from different severities, etc.
// See bugs 720180 and 800510.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-repeated-messages.html";

function test() {
  const PREF = "devtools.webconsole.persistlog";
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  // Check that css warnings are not coalesced if they come from different lines.
  info("waiting for 2 css warnings");

  waitForMessages({
    webconsole: hud,
    messages: [{
      name: "two css warnings",
      category: CATEGORY_CSS,
      count: 2,
      repeats: 1,
    }],
  }).then(testCSSRepeats.bind(null, hud));
}

function testCSSRepeats(hud) {
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    info("wait for repeats after page reload");

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "two css warnings, repeated twice",
        category: CATEGORY_CSS,
        repeats: 2,
        count: 2,
      }],
    }).then(testCSSRepeatsAfterReload.bind(null, hud));
  }, true);
  content.location.reload();
}

function testCSSRepeatsAfterReload(hud) {
  hud.jsterm.clearOutput(true);
  content.wrappedJSObject.testConsole();

  info("wait for repeats with the console API");

  waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "console.log 'foo repeat' repeated twice",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 2,
      },
      {
        name: "console.log 'foo repeat' repeated once",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 1,
      },
      {
        name: "console.error 'foo repeat' repeated once",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        repeats: 1,
      },
    ],
  }).then(testConsoleRepeats.bind(null, hud));
}

function testConsoleRepeats(hud) {
  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("undefined");
  content.console.log("undefined");

  info("make sure console API messages are not coalesced with jsterm output");

  waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "'undefined' jsterm input message",
        text: "undefined",
        category: CATEGORY_INPUT,
      },
      {
        name: "'undefined' jsterm output message",
        text: "undefined",
        category: CATEGORY_OUTPUT,
      },
      {
        name: "'undefined' console.log message",
        text: "undefined",
        category: CATEGORY_WEBDEV,
        repeats: 1,
      },
    ],
  }).then(finishTest);
}
