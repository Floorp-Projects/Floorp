// Return a promise with a reference to jsterm, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.
function getSplitConsole(dbg) {
  const { toolbox, win } = dbg;

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  });

  if (!win) {
    win = toolbox.win;
  }

  if (!toolbox.splitConsole) {
    pressKey(dbg, "Escape");
  }

  return new Promise(resolve => {
    toolbox.getPanelWhenReady("webconsole").then(() => {
      ok(toolbox.splitConsole, "Split console is shown.");
      let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
      resolve(jsterm);
    });
  });
}

add_task(function* () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = yield initDebugger("doc-script-switching.html");

  yield selectSource(dbg, "switching-01");

  // open the console
  yield getSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  // close the console
  clickElement(dbg, "codeMirror");
  // First time to focus out of text area
  pressKey(dbg, "Escape");
  // Second time to hide console
  pressKey(dbg, "Escape");
  ok(!dbg.toolbox.splitConsole, "Split console is hidden.");
});
