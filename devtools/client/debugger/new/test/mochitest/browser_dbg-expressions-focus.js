/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Ensures the input is displayed and focused when "+" is clicked
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  info(">> Close the panel");
  clickElementWithSelector(dbg, ".watch-expressions-pane ._header");

  info(">> Click + to add the new expression");
  await waitForElementWithSelector(
    dbg,
    ".watch-expressions-pane ._header .plus"
  );
  clickElementWithSelector(dbg, ".watch-expressions-pane ._header .plus");

  info(">> Ensure element gets focused");
  await waitForElementWithSelector(dbg, ".expression-input-container.focused");

  info(">> Ensure the element is focused");
  is(
    dbg.win.document.activeElement.classList.contains("input-expression"),
    true
  );
});
