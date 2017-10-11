/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test WebAssembly source maps
 */
add_task(async function() {
  const dbg = await initDebugger("doc-wasm-sourcemaps.html");

  await reload(dbg);
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  await waitForSource(dbg, "wasm-sourcemaps/average.c");
  await addBreakpoint(dbg, "wasm-sourcemaps/average.c", 12);

  clickElement(dbg, "resume");

  await waitForPaused(dbg);
  assertPausedLocation(dbg);

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
