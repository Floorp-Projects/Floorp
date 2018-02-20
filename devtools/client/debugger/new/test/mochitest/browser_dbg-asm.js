add_task(async function() {
  const dbg = await initDebugger("doc-asm.html");
  await reload(dbg);

  // After reload() we are getting getSources notifiction for old sources,
  // using the debugger statement to really stop are reloaded page.
  await waitForPaused(dbg);
  await resume(dbg);

  await waitForSources(dbg, "doc-asm.html", "asm.js");

  // Expand nodes and make sure more sources appear.
  is(findAllElements(dbg, "sourceNodes").length, 2);

  await clickElement(dbg, "sourceDirectoryLabel", 2);
  is(findAllElements(dbg, "sourceNodes").length, 4);

  await selectSource(dbg, "asm.js");

  await addBreakpoint(dbg, "asm.js", 7);
  invokeInTab("runAsm");

  await waitForPaused(dbg);
  assertPausedLocation(dbg, "asm.js", 7);
});
