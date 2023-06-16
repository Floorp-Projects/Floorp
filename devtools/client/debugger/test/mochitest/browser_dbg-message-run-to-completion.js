/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that messages from postMessage calls are not delivered while paused in
// the debugger.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-message-run-to-completion.html");
  invokeInTab("test", "doc-message-run-to-completion.html");
  await waitForPaused(dbg);
  let result = await dbg.client.evaluate("event.data");
  is(result.result, "first", "first message delivered in order");
  await resume(dbg);
  await waitForPaused(dbg);
  result = await dbg.client.evaluate("event.data");
  is(result.result, "second", "second message delivered in order");
  await resume(dbg);
  await waitForPaused(dbg);
  result = await dbg.client.evaluate("event.data");
  is(result.result, "third", "third message delivered in order");
});
