/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
 requestLongerTimeout(2);

/**
 * Test WebAssembly source maps
 */
add_task(async function() {
  const dbg = await initDebugger("doc-wasm-sourcemaps.html");
  await reload(dbg);

  // After reload() we are getting getSources notifiction for old sources,
  // using the debugger statement to really stop are reloaded page.
  await waitForPaused(dbg);
  await resume(dbg);

  await waitForSources(dbg, "doc-wasm-sourcemaps.html", "fib.c");

  // Set breakpoint and reload the page.
  ok(true, "Original sources exist");
  await selectSource(dbg, "fib.c");
  await addBreakpoint(dbg, "fib.c", 10);
  reload(dbg);

  // The same debugger statement as above, but using at for
  // workaround to break at original source (see below) and not generated.
  await waitForPaused(dbg);
  await selectSource(dbg, "fib.c");
  resume(dbg);

  await waitForPaused(dbg, "fib.c");

  const frames = findAllElements(dbg, "frames");
  const firstFrameTitle = frames[0].querySelector(".title").textContent;
  is(firstFrameTitle, "(wasmcall)", "It shall be a wasm call");
  const firstFrameLocation = frames[0].querySelector(".location").textContent;
  is(
    firstFrameLocation.includes("fib.c"),
    true,
    "It shall be fib.c source"
  );
});
