/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-837351-security-errors.html";

add_task(function* () {
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

