/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that server log appears in the console panel - bug 1168872
let test = asyncTest(function* () {
  const PREF = "devtools.webconsole.filter.serverlog";
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-server-logging.sjs";

  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  BrowserReload();

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
});
