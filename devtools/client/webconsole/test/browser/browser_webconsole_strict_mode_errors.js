/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that "use strict" JS errors generate errors, not warnings.

"use strict";

add_task(async function () {
  const hud = await openNewTabAndConsole(
    "data:text/html;charset=utf8,<!DOCTYPE html>empty page"
  );

  await loadScriptURI("'use strict';var arguments;");
  await waitForError(
    hud,
    "SyntaxError: 'arguments' can't be defined or assigned to in strict mode code"
  );

  await loadScriptURI("'use strict';function f(a, a) {};");
  await waitForError(hud, "SyntaxError: duplicate formal argument a");

  await loadScriptURI("'use strict';var o = {get p() {}};o.p = 1;");
  await waitForError(hud, 'TypeError: setting getter-only property "p"');

  await loadScriptURI("'use strict';v = 1;");
  await waitForError(
    hud,
    "ReferenceError: assignment to undeclared variable v"
  );
});

async function waitForError(hud, text) {
  await waitFor(() => findErrorMessage(hud, text));
  ok(true, "Received expected error message");
}

function loadScriptURI(script) {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }
  const uri =
    "data:text/html;charset=utf8,<!DOCTYPE html><script>" +
    script +
    "</script>";
  return navigateTo(uri);
}
