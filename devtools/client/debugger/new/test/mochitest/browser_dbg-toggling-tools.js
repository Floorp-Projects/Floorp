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

add_task(function*() {
  const dbg = yield initDebugger("doc-scripts.html");

  yield selectSource(dbg, "long");
  dbg.win.cm.scrollTo(0, 284);

  pressKey(dbg, "inspector");
  pressKey(dbg, "debugger");

  is(dbg.win.cm.getScrollInfo().top, 284);
});
