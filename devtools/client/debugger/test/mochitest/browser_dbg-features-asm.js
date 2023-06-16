/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test covers all specifics of debugging ASM.js files.
 *
 * ASM.js is a subset of the Javascript syntax.
 * Thanks to these limitations, the JS engine is able to compile this code
 * into machine instructions and execute it faster.
 *
 * When the DevTools are opened, ThreadConfiguration's `observeAsmJS` is set to true,
 * which sets DebuggerAPI's `allowUnobservedAsmJS` to false,
 * which disables the compilation of ASM.js files and make them run as regular JS code.
 * Thus, allowing to debug them as regular JS code.
 *
 * This behavior introduces some limitations when opening the debugger against
 * and already loaded page. The ASM.js file won't be debuggable.
 */

"use strict";

add_task(async function () {
  // Load the test page before opening the debugger
  // and also force a GC before opening the debugger
  // so that ASM.js is fully garbaged collected.
  // Otherwise on debug builds, the thread actor is able to intermittently
  // retrieve the ASM.js sources and retrieve the breakable lines.
  const tab = await addTab(EXAMPLE_URL + "doc-asm.html");
  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    Cu.forceGC();
  });
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  // There is the legit and the spurious wasm source (see following comment)
  await waitForSourcesInSourceTree(dbg, ["doc-asm.html", "asm.js", "asm.js"]);
  is(dbg.selectors.getSourceCount(), 3, "There are only three sources");

  const legitSource = findSource(dbg, EXAMPLE_URL + "asm.js");
  ok(
    legitSource.url.startsWith("https://"),
    "We got the legit source that works, not the spurious WASM one"
  );
  is(legitSource.isWasm, false, "ASM.js sources are *not* flagged as WASM");

  // XXX Bug 1759573 - There is a spurious wasm source reported which is broken
  // and ideally shouldn't exists at all in UI.
  // The Thread Actor is notified by the Debugger API about a WASM
  // source when calling Debugger.findSources in ThreadActor.addAllSources.
  const wasmUrl = "wasm:" + legitSource.url;
  ok(
    sourceExists(dbg, wasmUrl),
    `There is a spurious wasm:// source displayed: ${wasmUrl}`
  );

  await selectSource(dbg, legitSource);

  assertTextContentOnLine(dbg, 7, "return 1 | 0;");

  info(
    "Before reloading, ThreadConfiguration's 'observedAsmJS' was false while the page was loading"
  );
  info(
    "So that we miss info about the ASM sources and lines are not breakables"
  );
  assertLineIsBreakable(dbg, legitSource.url, 7, false);

  info("Reload and assert that ASM.js file are then debuggable");
  await reload(dbg, "doc-asm.html", "asm.js");

  info("After reloading, ASM lines are breakable");
  // Ensure selecting the source before asserting breakable lines
  // otherwise the gutter may not be yet updated
  await selectSource(dbg, "asm.js");
  assertLineIsBreakable(dbg, legitSource.url, 7, true);

  await waitForSourcesInSourceTree(dbg, ["doc-asm.html", "asm.js"]);
  is(dbg.selectors.getSourceCount(), 2, "There is only the two sources");

  assertTextContentOnLine(dbg, 7, "return 1 | 0;");

  await addBreakpoint(dbg, "asm.js", 7);
  invokeInTab("runAsm");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "asm.js").id, 7);
  await assertBreakpoint(dbg, 7);

  await removeBreakpoint(dbg, findSource(dbg, "asm.js").id, 7);
  await resume(dbg);
});
