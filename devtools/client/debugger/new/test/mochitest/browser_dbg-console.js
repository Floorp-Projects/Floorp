add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

  // open the console
  await getSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  // close the console
  await clickElement(dbg, "codeMirror");
  // First time to focus out of text area
  pressKey(dbg, "Escape");

  // Second time to hide console
  pressKey(dbg, "Escape");
  ok(!dbg.toolbox.splitConsole, "Split console is hidden.");
});
