/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that pausing within an event handler on an element does *not* show the
// HTML page containing that element. It should show a sources tab containing
// just the handler's text instead.
add_task(async function() {
  const dbg = await initDebugger("doc-event-handler.html");

  invokeInTab("synthesizeClick");

  await waitForPaused(dbg);

  const state = dbg.store.getState();
  const source = dbg.selectors.getSelectedSource(state);
  ok(!source.url, "Selected source should not have a URL");
});
