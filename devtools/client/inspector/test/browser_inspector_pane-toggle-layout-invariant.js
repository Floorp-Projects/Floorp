/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that documents with suppressed whitespace nodes are unaffected by the
// activation of the inspector panel.

const TEST_URL = URL_ROOT + "doc_inspector_pane-toggle-layout-invariant.html";

async function getInvariantRect() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    const invariant = content.document.getElementById("invariant");
    return invariant.getBoundingClientRect();
  });
}

add_task(async function() {
  await addTab(TEST_URL);

  // Get the initial position of the "invariant" element. We'll later check it
  // again and we'll expect to get the same value.
  const beforeRect = await getInvariantRect();

  // Open the inspector.
  await openInspector();

  const afterRect = await getInvariantRect();

  is(afterRect.x, beforeRect.x, "invariant x should be same as initial value.");
  is(afterRect.y, beforeRect.y, "invariant y should be same as initial value.");
});
