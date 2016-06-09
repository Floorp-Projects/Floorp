/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure messages are not considered repeated when coming from
// different lines of code, or from different severities, etc.
// See bugs 720180 and 800510.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-repeated-messages.html";
const PREF = "devtools.webconsole.persistlog";

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield consoleOpened(hud);

  let loaded = loadBrowser(browser);
  BrowserReload();
  yield loaded;

  yield testCSSRepeats(hud);
  yield testCSSRepeatsAfterReload(hud);
  yield testConsoleRepeats(hud);
  yield testConsoleFalsyValues(hud);

  Services.prefs.clearUserPref(PREF);
});

function consoleOpened(hud) {
  // Check that css warnings are not coalesced if they come from different
  // lines.
  info("waiting for 2 css warnings");

  return waitForMessages({
    webconsole: hud,
    messages: [{
      name: "two css warnings",
      category: CATEGORY_CSS,
      count: 2,
      repeats: 1,
    }],
  });
}

function testCSSRepeats(hud) {
  info("wait for repeats after page reload");

  return waitForMessages({
    webconsole: hud,
    messages: [{
      name: "two css warnings, repeated twice",
      category: CATEGORY_CSS,
      repeats: 2,
      count: 2,
    }],
  });
}

function testCSSRepeatsAfterReload(hud) {
  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("testConsole()");

  info("wait for repeats with the console API");

  return waitForMessages({
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
  });
}

function testConsoleRepeats(hud) {
  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("undefined");

  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.console.log("undefined");
  });

  info("make sure console API messages are not coalesced with jsterm output");

  return waitForMessages({
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
  });
}

function testConsoleFalsyValues(hud) {
  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("testConsoleFalsyValues()");

  info("wait for repeats of falsy values with the console API");

  return waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "console.log 'NaN' repeated once",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 1,
      },
      {
        name: "console.log 'undefined' repeated once",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 1,
      },
      {
        name: "console.log 'null' repeated once",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 1,
      },
      {
        name: "console.log 'NaN' repeated twice",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 2,
      },
      {
        name: "console.log 'undefined' repeated twice",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 2,
      },
      {
        name: "console.log 'null' repeated twice",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        repeats: 2,
      },
    ],
  });
}
