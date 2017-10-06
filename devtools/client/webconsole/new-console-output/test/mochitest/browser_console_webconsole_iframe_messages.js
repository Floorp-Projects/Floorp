/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

add_task(function* () {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  ok(hud, "web console opened");

  yield testWebConsole(hud);
  yield closeConsole();
  info("web console closed");

  hud = yield HUDService.toggleBrowserConsole();
  yield testBrowserConsole(hud);
  yield closeConsole();
});

function* testWebConsole(hud) {
  yield waitForMessages({
    webconsole: hud,
    messages: expectedMessages,
  });

  info("first messages matched");

  yield waitForMessages({
    webconsole: hud,
    messages: expectedMessagesAny,
    matchCondition: "any",
  });
}

function* testBrowserConsole(hud) {
  ok(hud, "browser console opened");

  // TODO: The browser console doesn't show page's console.log statements
  // in e10s windows. See Bug 1241289.
  if (Services.appinfo.browserTabsRemoteAutostart) {
    todo(false, "Bug 1241289");
    return;
  }

  yield waitForMessages({
    webconsole: hud,
    messages: expectedMessages,
  });

  info("first messages matched");
  yield waitForMessages({
    webconsole: hud,
    messages: expectedMessagesAny,
    matchCondition: "any",
  });
}
