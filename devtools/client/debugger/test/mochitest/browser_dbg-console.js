/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

  // open the console
  await getDebuggerSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  // close the console
  await clickElement(dbg, "codeMirror");
  // First time to focus out of text area
  pressKey(dbg, "Escape");

  // Second time to hide console
  pressKey(dbg, "Escape");
  ok(!dbg.toolbox.splitConsole, "Split console is hidden.");
});
