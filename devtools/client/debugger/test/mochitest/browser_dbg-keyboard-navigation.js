/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that keyboard navigation into and out of debugger code editor

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");
  let doc = dbg.win.document;

  await selectSource(dbg, "simple2");

  await waitForElementWithSelector(dbg, ".CodeMirror");
  findElementWithSelector(dbg, ".CodeMirror").focus();

  // Enter code editor
  pressKey(dbg, "Enter");
  is(
    findElementWithSelector(dbg, "textarea"),
    doc.activeElement,
    "Editor is enabled"
  );

  // Exit code editor and focus on container
  pressKey(dbg, "Escape");
  is(
    findElementWithSelector(dbg, ".CodeMirror"),
    doc.activeElement,
    "Focused on container"
  );

  // Enter code editor
  pressKey(dbg, "Tab");
  is(
    findElementWithSelector(dbg, "textarea"),
    doc.activeElement,
    "Editor is enabled"
  );
});
