
add_task(async function() {
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("ember/quickstart/dist/");

  await invokeWithBreakpoint(dbg, "mapTestFunction", "quickstart/router.js", { line: 13, column: 2 }, async () => {
    await assertScopes(dbg, [
      "Module",
      ["config", "{\u2026}"],
      "EmberRouter:Class()",
      "Router:Class()",
    ]);
  });
});
