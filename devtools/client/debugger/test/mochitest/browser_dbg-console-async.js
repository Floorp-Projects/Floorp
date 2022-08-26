/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Return a promise with a reference to jsterm, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.

"use strict";

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  Services.prefs.setBoolPref(
    "devtools.debugger.features.map-await-expression",
    true
  );

  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  await selectSource(dbg, "script-switching-01.js");

  // open the console
  await getDebuggerSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  const webConsole = await dbg.toolbox.getPanel("webconsole");
  const wrapper = webConsole.hud.ui.wrapper;

  wrapper.dispatchEvaluateExpression(`
    let sleep = async (time, v) => new Promise(
      res => setTimeout(() => res(v+'!!!'), time)
    )
  `);

  await hasMessageByType(dbg, "sleep", ".command");

  wrapper.dispatchEvaluateExpression(`await sleep(200, "DONE")`);
  await hasMessageByType(dbg, "DONE!!!", ".result");
});

async function hasMessageByType(dbg, msg, typeSelector) {
  const webConsole = await dbg.toolbox.getPanel("webconsole");
  const hud = webConsole.hud;
  return waitFor(
    async () => !!findMessagesByType(hud, msg, typeSelector).length
  );
}
