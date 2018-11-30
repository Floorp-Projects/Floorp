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

function findMessages(win, query) {
  return Array.prototype.filter.call(
    win.document.querySelectorAll(".message"),
    e => e.innerText.includes(query)
  )
}

async function hasMessage(dbg, msg) {
  const webConsole = await dbg.toolbox.getPanel("webconsole")
  return waitFor(async () => findMessages(
    webConsole._frameWindow,
    msg
  ).length > 0)
}

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  Services.prefs.setBoolPref("devtools.debugger.features.map-await-expression", true);

  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

  // open the console
  await getSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  const webConsole = await dbg.toolbox.getPanel("webconsole")
  const jsterm = webConsole.hud.jsterm;

  await jsterm.execute(`
    let sleep = async (time, v) => new Promise(
      res => setTimeout(() => res(v+'!!!'), time)
    )
  `);

  await hasMessage(dbg, "sleep");

  await jsterm.execute(`await sleep(200, "DONE")`);
  await hasMessage(dbg, "DONE!!!");
});
