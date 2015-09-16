/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that server log appears in the console panel - bug 1168872
var test = asyncTest(function* () {
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-server-logging.sjs";

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
  })

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