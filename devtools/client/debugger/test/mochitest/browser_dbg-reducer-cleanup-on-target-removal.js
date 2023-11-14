/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Verify that reducers are cleared on target removal

"use strict";

const testServer = createVersionizedHttpTestServer(
  "examples/sourcemaps-reload-uncompressed"
);
const IFRAME_TEST_URL = testServer.urlFor("index.html");

// Embed the integration test page into an iframe
const TEST_URI = `https://example.com/document-builder.sjs?html=
<iframe src="${encodeURI(IFRAME_TEST_URL)}"></iframe><body>`;

add_task(async function () {
  if (!isEveryFrameTargetEnabled()) {
    ok(true, "This test is disabled without EFT");
    return;
  }
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URI);

  const frameBC = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      return content.document.querySelector("iframe").browsingContext;
    }
  );

  info("Pause on an original source to ensure filling the reducers");
  await waitForSource(dbg, "original.js");
  await selectSource(dbg, "original.js");
  await addBreakpoint(dbg, "original.js", 8);

  // The following task will never resolve as the call to foo()
  // will be paused and never released
  PromiseTestUtils.allowMatchingRejectionsGlobally(
    /Actor 'SpecialPowers' destroyed before query 'Spawn'/
  );
  SpecialPowers.spawn(frameBC, [], function () {
    return content.wrappedJSObject.foo();
  });

  await waitForPaused(dbg, "original.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "original.js").id, 8);
  // Also open the genertated source to populate the reducer for original and generated sources
  await dbg.actions.jumpToMappedSelectedLocation();
  await waitForSelectedSource(dbg, "bundle.js");

  // Assert that reducer do have some data before remove the target.
  ok(dbg.selectors.getSourceCount() > 0, "Some sources exists");
  is(dbg.selectors.getBreakpointCount(), 1, "There is one breakpoint");
  is(dbg.selectors.getSourceTabs().length, 2, "Two tabs are opened");
  ok(
    dbg.selectors.getAllThreads().length > 1,
    "There is many targets/threads involved by the intergration test"
  );
  ok(
    !!dbg.selectors.getSourcesTreeSources().length,
    "There is sources displayed in the SourceTree"
  );
  is(
    dbg.selectors.getSelectedLocation().source.id,
    findSource(dbg, "bundle.js").id,
    "The generated source is reporeted as selected"
  );
  ok(!!dbg.selectors.getFocusedSourceItem(), "Has a focused source tree item");
  ok(
    dbg.selectors.getExpandedState().size > 0,
    "Has some expanded source tree items"
  );

  // But also directly querying the reducer states, as we don't necessarily
  // have selectors exposing their raw internals
  let state = dbg.store.getState();
  ok(
    !!Object.keys(state.ast.mutableOriginalSourcesSymbols).length,
    "Some symbols for original sources exists"
  );
  ok(
    !!Object.keys(state.ast.mutableSourceActorSymbols).length,
    "Some symbols for generated sources exists"
  );
  ok(!!Object.keys(state.ast.mutableInScopeLines).length, "Some scopes exists");
  ok(
    state.sourceActors.mutableSourceActors.size > 0,
    "Some source actor exists"
  );
  ok(
    state.sourceActors.mutableBreakableLines.size > 0,
    "Some breakable line exists"
  );
  ok(
    state.sources.mutableBreakpointPositions.size > 0,
    "Some breakable positions exists"
  );
  ok(
    state.sources.mutableOriginalBreakableLines.size > 0,
    "Some original breakable lines exists"
  );

  info("Remove the iframe");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("iframe").remove();
  });

  info("Wait for all sources to be removed");
  await waitFor(() => dbg.selectors.getSourceCount() == 0);
  // The pause thread being removed, we are no longer paused.
  assertNotPaused(dbg);

  // Assert they largest reducer data is cleared on thread removal

  // First via common selectors
  is(dbg.selectors.getSourceCount(), 0, "No sources exists");
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoints exists");
  is(dbg.selectors.getSourceTabs().length, 0, "No tabs exists");
  is(
    dbg.selectors.getAllThreads().length,
    1,
    "There is only the top level target/thread"
  );
  is(
    dbg.selectors.getSourcesTreeSources().length,
    0,
    "There is no source in the SourceTree"
  );
  is(
    dbg.selectors.getSelectedLocation(),
    null,
    "Selected location is nullified as the selected source has been removed"
  );
  ok(!dbg.selectors.getFocusedSourceItem(), "No more focused source tree item");
  is(
    dbg.selectors.getExpandedState().size,
    0,
    "No more expanded source tree items"
  );

  // But also directly querying the reducer states, as we don't necessarily
  // have selectors exposing their raw internals
  state = dbg.store.getState();
  is(
    Object.keys(state.ast.mutableOriginalSourcesSymbols).length,
    0,
    "No symbols for original sources exists"
  );
  is(
    Object.keys(state.ast.mutableSourceActorSymbols).length,
    0,
    "No symbols for generated sources exists"
  );
  is(Object.keys(state.ast.mutableInScopeLines).length, 0, "No scopes exists");
  is(state.sourceActors.mutableSourceActors.size, 0, "No source actor exists");
  is(
    state.sourceActors.mutableBreakableLines.size,
    0,
    "No breakable line exists"
  );
  is(
    state.sources.mutableBreakpointPositions.size,
    0,
    "No breakable positions exists"
  );
  is(
    state.sources.mutableOriginalBreakableLines.size,
    0,
    "No original breakable lines exists"
  );
});
