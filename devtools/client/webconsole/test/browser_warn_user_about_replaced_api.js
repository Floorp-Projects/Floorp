/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_REPLACED_API_URI = "http://example.com/browser/devtools/client/" +
                              "webconsole/test/test-console-replaced-api.html";
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/testscript.js";
const PREF = "devtools.webconsole.persistlog";

var test = asyncTest(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let { browser } = yield loadTab(TEST_URI);
  let hud = yield openConsole();

  yield testWarningNotPresent(hud);

  let loaded = loadBrowser(browser);
  content.location = TEST_REPLACED_API_URI;
  yield loaded;

  let hud2 = yield openConsole();

  yield testWarningPresent(hud2);

  Services.prefs.clearUserPref(PREF);
});

function testWarningNotPresent(hud) {
  let deferred = promise.defer();

  is(hud.outputNode.textContent.indexOf("logging API"), -1,
     "no warning displayed");

  // Bug 862024: make sure the warning doesn't show after page reload.
  info("reload " + TEST_URI);
  executeSoon(() => content.location.reload());

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testscript.js",
      category: CATEGORY_NETWORK,
    }],
  }).then(() => executeSoon(() => {
    is(hud.outputNode.textContent.indexOf("logging API"), -1,
       "no warning displayed");
    closeConsole().then(deferred.resolve);
  }));

  return deferred.promise;
}

function testWarningPresent(hud) {
  info("wait for the warning to show");
  let deferred = promise.defer();

  let warning = {
    webconsole: hud,
    messages: [{
      text: /logging API .+ disabled by a script/,
      category: CATEGORY_JS,
      severity: SEVERITY_WARNING,
    }],
  };

  waitForMessages(warning).then(() => {
    hud.jsterm.clearOutput();

    executeSoon(() => {
      info("reload the test page and wait for the warning to show");
      waitForMessages(warning).then(deferred.resolve);
      content.location.reload();
    });
  });

  return deferred.promise;
}
