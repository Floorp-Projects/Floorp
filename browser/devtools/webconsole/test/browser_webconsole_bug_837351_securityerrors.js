/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-837351-security-errors.html";

let test = asyncTest(function* () {
  yield pushPrefEnv();

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let button = hud.ui.rootElement.querySelector(".webconsole-filter-button[category=\"security\"]");
  ok(button, "Found security button in the web console");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Logged blocking mixed active content",
        text: "Blocked loading mixed active content \"http://example.com/\"",
        category: CATEGORY_SECURITY,
        severity: SEVERITY_ERROR
      },
    ],
  });
});

function pushPrefEnv() {
  let deferred = promise.defer();
  let options = {
    set: [["security.mixed_content.block_active_content", true]]
  };
  SpecialPowers.pushPrefEnv(options, deferred.resolve);
  return deferred.promise;
}

