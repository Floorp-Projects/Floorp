/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Tests that errors about insecure passwords are logged
 * to the web console
 */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
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

let test = asyncTest(function* () {
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
