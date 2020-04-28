/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test the "go to line" feature correctly responses to keyboard shortcuts.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "long.js");
  await selectSource(dbg, "long");
  await waitForSelectedSource(dbg, "long");

  info('Test opening');
  pressKey(dbg, "goToLine");
  assertEnabled(dbg);
  is(
    dbg.win.document.activeElement.tagName,
    "INPUT",
    "The input area of 'go to line' box is focused"
  );

  info('Test closing by the same keyboard shortcut');
  pressKey(dbg, "goToLine");
  assertDisabled(dbg);
  is(findElement(dbg, "searchField"), null, "The 'go to line' box is closed");

  info('Test closing by escape');
  pressKey(dbg, "goToLine");
  assertEnabled(dbg);

  pressKey(dbg, "Escape");
  assertDisabled(dbg);
  is(findElement(dbg, "searchField"), null, "The 'go to line' box is closed");

  info('Test going to the correct line');
  pressKey(dbg, "goToLine");
  await waitForGoToLineBoxFocus(dbg);
  type(dbg, "66");
  pressKey(dbg, "Enter");
  assertLine(dbg, 66);
});

function assertEnabled(dbg) {
  is(dbg.selectors.getQuickOpenEnabled(), true, "quickOpen enabled");
}

function assertDisabled(dbg) {
  is(dbg.selectors.getQuickOpenEnabled(), false, "quickOpen disabled");
}

async function waitForGoToLineBoxFocus(dbg) {
  await waitFor(() => dbg.win.document.activeElement.tagName === "INPUT");
}

function assertLine(dbg, lineNumber) {
  is(
    dbg.selectors.getSelectedLocation().line,
    lineNumber,
    `goto line is ${lineNumber}`
  );
}
