/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors about insecure passwords are logged to the web console.
// See Bug 762593.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-insecure-passwords-about-blank-web-console-warning.html";
const INSECURE_PASSWORD_MSG =
  "Password fields present on an insecure (http://) iframe." +
  " This is a security risk that allows user login credentials to be stolen.";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findMessage(hud, INSECURE_PASSWORD_MSG, ".message.warn"),
    "",
    100
  );
  ok(true, "Insecure password error displayed successfully");
});
