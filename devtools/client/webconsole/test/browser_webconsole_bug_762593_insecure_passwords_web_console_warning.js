/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 // Tests that errors about insecure passwords are logged to the web console.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-762593-insecure-passwords-web-" +
                 "console-warning.html";
const INSECURE_PASSWORD_MSG = "Password fields present on an insecure " +
                 "(http://) page. This is a security risk that allows user " +
                 "login credentials to be stolen.";
const INSECURE_FORM_ACTION_MSG = "Password fields present in a form with an " +
                 "insecure (http://) form action. This is a security risk " +
                 "that allows user login credentials to be stolen.";
const INSECURE_IFRAME_MSG = "Password fields present on an insecure " +
                 "(http://) iframe. This is a security risk that allows " +
                 "user login credentials to be stolen.";
const INSECURE_PASSWORDS_URI = "https://developer.mozilla.org/docs/Security/" +
                 "InsecurePasswords";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let result = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Insecure password error displayed successfully",
        text: INSECURE_PASSWORD_MSG,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING
      },
      {
        name: "Insecure iframe error displayed successfully",
        text: INSECURE_IFRAME_MSG,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING
      },
      {
        name: "Insecure form action error displayed successfully",
        text: INSECURE_FORM_ACTION_MSG,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING
      },
    ],
  });

  yield testClickOpenNewTab(hud, result);
});

function testClickOpenNewTab(hud, [result]) {
  let msg = [...result.matched][0];
  let warningNode = msg.querySelector(".learn-more-link");
  ok(warningNode, "learn more link");
  return simulateMessageLinkClick(warningNode, INSECURE_PASSWORDS_URI);
}
