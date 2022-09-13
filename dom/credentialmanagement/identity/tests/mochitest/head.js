/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var idp_host = "https://example.net";
var test_path = "/tests/dom/credentialmanagement/identity/tests/mochitest";
var idp_api = idp_host + test_path;

async function setupTest(testName) {
  ok(
    window.location.pathname.includes(testName),
    `Must set the right test name when setting up. Test name "${testName}" must be in URL path "${window.location.pathname}"`
  );
  let fetchPromise = fetch(
    `${idp_api}/server_manifest.sjs?set_test=${testName}`
  );
  let focusPromise = SimpleTest.promiseFocus();
  window.open(`${idp_api}/helper_set_cookie.html`, "_blank");
  await focusPromise;
  return fetchPromise;
}
