/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that pausing within an event handler on an element does *not* show the
// HTML page containing that element. It should show a sources tab containing
// just the handler's text instead.
add_task(async function() {
  const dbg = await initDebugger("doc-event-handler.html");

  invokeInTab("synthesizeClick");
  await waitForPaused(dbg);
  const source = dbg.selectors.getSelectedSource();
  ok(!source.url, "Selected source should not have a URL");
});
