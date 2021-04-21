/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Confirms that a conditional panel is opened at the
// correct location in generated files.
add_task(async function() {
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  await selectSource(dbg, "bundle.js");
  getCM(dbg).scrollIntoView({ line: 55, ch: 0 });

  await setLogPoint(dbg, 55);
  await waitForConditionalPanelFocus(dbg);
  ok(
    !!getConditionalPanel(dbg, 55),
    "conditional panel panel is open on line 55"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    55,
    "conditional panel location is line 55"
  );
});

async function setLogPoint(dbg, index) {
  const gutterEl = await getEditorLineGutter(dbg, index);
  rightClickEl(dbg, gutterEl);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );
}

async function waitForConditionalPanelFocus(dbg) {
  await waitFor(() => dbg.win.document.activeElement.tagName === "TEXTAREA");
}

function getConditionalPanel(dbg, line) {
  return getCM(dbg).doc.getLineHandle(line - 1).widgets[0];
}
