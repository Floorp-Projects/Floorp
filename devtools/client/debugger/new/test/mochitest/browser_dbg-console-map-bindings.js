// Return a promise with a reference to jsterm, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.
function getSplitConsole(dbg) {
  const { toolbox, win } = dbg;

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

async function evaluate(dbg, expression) {
  const { toolbox } = dbg;
  let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
  const msg = await jsterm.execute(expression);
  return msg.innerText;
}

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger("doc-strict.html");

  await getSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  invokeInTab("strict", 2);

  await waitForPaused(dbg);
  await evaluate(dbg, "var c = 3");
  const msg2 = await evaluate(dbg, "c");

  is(msg2.trim(), "3");
});
