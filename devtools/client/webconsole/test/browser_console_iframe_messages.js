/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that cached messages from nested iframes are displayed in the
// Web/Browser Console.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-consoleiframes.html";

const expectedMessages = [
  {
    text: "main file",
    category: CATEGORY_WEBDEV,
    severity: SEVERITY_LOG,
  },
  {
    text: "blah",
    category: CATEGORY_JS,
    severity: SEVERITY_ERROR
  },
  {
    text: "iframe 2",
    category: CATEGORY_WEBDEV,
    severity: SEVERITY_LOG
  },
  {
    text: "iframe 3",
    category: CATEGORY_WEBDEV,
    severity: SEVERITY_LOG
  }
];

// "iframe 1" console messages can be coalesced into one if they follow each
// other in the sequence of messages (depending on timing). If they do not, then
// they will be displayed in the console output independently, as separate
// messages. This is why we need to match any of the following two rules.
const expectedMessagesAny = [
  {
    name: "iframe 1 (count: 2)",
    text: "iframe 1",
    category: CATEGORY_WEBDEV,
    severity: SEVERITY_LOG,
    count: 2
  },
  {
    name: "iframe 1 (repeats: 2)",
    text: "iframe 1",
    category: CATEGORY_WEBDEV,
    severity: SEVERITY_LOG,
    repeats: 2
  },
];

function test() {
  expectUncaughtException();
  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}

function consoleOpened(hud) {
  ok(hud, "web console opened");

  waitForMessages({
    webconsole: hud,
    messages: expectedMessages,
  }).then(() => {
    info("first messages matched");
    waitForMessages({
      webconsole: hud,
      messages: expectedMessagesAny,
      matchCondition: "any",
    }).then(() => {
      closeConsole().then(onWebConsoleClose);
    });
  });
}

function onWebConsoleClose() {
  info("web console closed");
  HUDService.toggleBrowserConsole().then(onBrowserConsoleOpen);
}

function onBrowserConsoleOpen(hud) {
  ok(hud, "browser console opened");
  waitForMessages({
    webconsole: hud,
    messages: expectedMessages,
  }).then(() => {
    info("first messages matched");
    waitForMessages({
      webconsole: hud,
      messages: expectedMessagesAny,
      matchCondition: "any",
    }).then(() => {
      closeConsole().then(finishTest);
    });
  });
}
