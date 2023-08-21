/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that console helpers are not set when existing variables with similar names
// exists in the page.

const TEST_URI = `data:text/html,<!DOCTYPE html><script>
    var $ = "var_$";
    const $$ = "const_$$";
    let help = "let_help";
    globalThis.values = "globalThis_values";
    function keys() {
      return "function_keys";
    }

    function pauseInDebugger() {
      let $x = "let_$x";
      debugger;
    }
  </script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForResultMessage(hud, "$", "var_$");
  ok(message, "$ was not overridden");

  message = await executeAndWaitForResultMessage(hud, "$$", "const_$$");
  ok(message, "$$ was not overridden");

  message = await executeAndWaitForResultMessage(hud, "help", "let_help");
  ok(message, "help was not overridden");

  message = await executeAndWaitForResultMessage(
    hud,
    "values",
    "globalThis_values"
  );
  ok(message, "values was not overridden");

  message = await executeAndWaitForResultMessage(
    hud,
    "keys()",
    "function_keys"
  );
  ok(message, "keys was not overridden");

  info("Pause in Debugger");
  await openDebugger();
  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);
  const dbg = createDebuggerContext(toolbox);
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.pauseInDebugger();
  });
  await pauseDebugger(dbg);

  info("Go back to Console");
  await toolbox.selectTool("webconsole");
  message = await executeAndWaitForResultMessage(hud, "$x", "let_$x");
  ok(
    message,
    `frame variable "$x" was not overridden while paused in the debugger`
  );

  await resume(dbg);
});

add_task(async function () {
  // Test the behavior when the variables are declared in the script.

  const hud = await openNewTabAndConsole("data:text/html,Nothing");

  let message = await executeAndWaitForResultMessage(
    hud,
    "var $ = 'var_$'; $",
    "var_$"
  );
  ok(message, "$ was not overridden");

  message = await executeAndWaitForResultMessage(
    hud,
    "const $$ = 'const_$$'; $$",
    "const_$$"
  );
  ok(message, "$$ was not overridden");

  message = await executeAndWaitForResultMessage(
    hud,
    "function keys() { return 'function_keys';  } keys()",
    "function_keys"
  );
  ok(message, "keys was not overridden");

  // Hoisted variable.
  message = await executeAndWaitForResultMessage(
    hud,
    "{ var clear = 'var_clear';  } clear",
    "var_clear"
  );
  ok(message, "clear was not overridden");

  // Hoisted function.
  message = await executeAndWaitForResultMessage(
    hud,
    "{ function help() { return 'function_help';  } } help()",
    "function_help"
  );
  ok(message, "help was not overridden");

  // Lexical variables are not hoisted.
  message = await executeAndWaitForResultMessage(
    hud,
    "{ let values = 'let_values';  } typeof values",
    "function"
  );
  ok(message, "values was not overridden because it's not global");
});

add_task(async function () {
  // NOTE: Open a new page to use the same binding name again.

  const hud = await openNewTabAndConsole("data:text/html,Nothing");

  // An assignment on `globalThis` property inside the script doesn't shadow
  // the helper binding.
  let message = await executeAndWaitForResultMessage(
    hud,
    "globalThis.values = 'globalThis_values'; typeof values",
    "function"
  );
  ok(message, "values was overridden because it's not declaration");

  // Executing `values` again shadows the helper binding, given it exists
  // before the execution.
  message = await executeAndWaitForResultMessage(
    hud,
    "values",
    "globalThis_values"
  );
  ok(message, "values was not overridden if it exists before the execution");
});
