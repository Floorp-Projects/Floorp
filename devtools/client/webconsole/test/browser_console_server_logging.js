/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that server log appears in the console panel - bug 1168872
add_task(function* () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test/test-console-server-logging.sjs";

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  // Set logging filter and wait till it's set on the backend
  hud.setFilterState("serverlog", true);
  yield updateServerLoggingListener(hud);

  BrowserReloadSkipCache();

  // Note that the test is also checking out the (printf like)
  // formatters and encoding of UTF8 characters (see the one at the end).
  let text = "values: string  Object { a: 10 }  123 1.12 \u2713";

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: text,
      category: CATEGORY_SERVER,
      severity: SEVERITY_LOG,
    }],
  });
  // Clean up filter
  hud.setFilterState("serverlog", false);
  yield updateServerLoggingListener(hud);
});

add_task(function* () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test/test-console-server-logging-array.sjs";

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  // Set logging filter and wait till it's set on the backend
  hud.setFilterState("serverlog", true);
  yield updateServerLoggingListener(hud);

  BrowserReloadSkipCache();
  // Note that the test is also checking out the (printf like)
  // formatters and encoding of UTF8 characters (see the one at the end).
  let text = "Object { best: \"Firefox\", reckless: \"Chrome\", " +
    "new_ie: \"Safari\", new_new_ie: \"Edge\" }";
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: text,
      category: CATEGORY_SERVER,
      severity: SEVERITY_LOG,
    }],
  });
  // Clean up filter
  hud.setFilterState("serverlog", false);
  yield updateServerLoggingListener(hud);
});

add_task(function* () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test/test-console-server-logging-backtrace.sjs";

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  // Set logging filter and wait till it's set on the backend
  hud.setFilterState("serverlog", true);
  yield updateServerLoggingListener(hud);

  BrowserReloadSkipCache();
  // Note that the test is also checking out the (printf like)
  // formatters and encoding of UTF8 characters (see the one at the end).
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "correct 1",
      category: CATEGORY_SERVER,
      severity: SEVERITY_ERROR,
      source: {url: "/some/path/to/file.py", line: 33}
    }, {
      text: "correct 2",
      category: CATEGORY_SERVER,
      severity: SEVERITY_ERROR,
      source: {url: "/some/path/to/file.py", line: 33}
    }, {
      text: "wrong 1",
      category: CATEGORY_SERVER,
      severity: SEVERITY_ERROR,
      source: {url: "/some/path/to/file.py:33wrong"}
    }, {
      text: "wrong 2",
      category: CATEGORY_SERVER,
      severity: SEVERITY_ERROR,
      source: {url: "/some/path/to/file.py"}
    }],
  });
  // Clean up filter
  hud.setFilterState("serverlog", false);
  yield updateServerLoggingListener(hud);
});

function updateServerLoggingListener(hud) {
  let deferred = promise.defer();
  hud.ui._updateServerLoggingListener(response => {
    deferred.resolve(response);
  });
  return deferred.promise;
}
