/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test WebAssembly source maps
 */
add_task(async function() {
  const dbg = await initDebugger("doc-wasm-sourcemaps.html");

  // NOTE: wait for page load -- attempt to fight the intermittent failure:
  // "A promise chain failed to handle a rejection: Debugger.Frame is not live"
  await waitForSource(dbg, "doc-wasm-sourcemaps");

  await waitForLoadedSources(dbg);
  await reload(dbg);
  await waitForPaused(dbg);

  await waitForLoadedSource(dbg, "doc-wasm-sourcemaps");
  assertPausedLocation(dbg);

  await waitForSource(dbg, "wasm-sourcemaps/average.c");
  await addBreakpoint(dbg, "wasm-sourcemaps/average.c", 12);

  clickElement(dbg, "resume");

  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "average.c");
  assertPausedLocation(dbg);
  toggleCallStack(dbg);

  const frames = findAllElements(dbg, "frames");
  const firstFrameTitle = frames[0].querySelector(".title").textContent;
  is(firstFrameTitle, "(wasmcall)", "It shall be a wasm call");
  const firstFrameLocation = frames[0].querySelector(".location").textContent;
  is(
    firstFrameLocation.includes("average.c"),
    true,
    "It shall be to avarage.c source"
  );
});
