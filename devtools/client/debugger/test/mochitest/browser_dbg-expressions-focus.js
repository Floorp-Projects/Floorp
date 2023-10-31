/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Ensures the input is displayed and focused when "+" is clicked

"use strict";

add_task(async function () {
  // Start with the watch expression panel open
  await pushPref("devtools.debugger.expressions-visible", true);
  const dbg = await initDebugger("doc-script-switching.html");

  const watchExpressionHeaderEl = findElement(dbg, "watchExpressionsHeader");
  is(
    watchExpressionHeaderEl.tagName,
    "BUTTON",
    "The accordion toggle is a proper button"
  );
  is(
    watchExpressionHeaderEl.getAttribute("aria-expanded"),
    "true",
    "The accordion toggle is properly marked as expanded"
  );

  info("Close the panel");
  clickDOMElement(dbg, watchExpressionHeaderEl);
  await waitFor(() =>
    watchExpressionHeaderEl.getAttribute("aria-expanded", "false")
  );
  ok(true, "The accordion toggle is properly marked as collapsed");

  info("Click + to add the new expression");
  await waitForElement(dbg, "watchExpressionsAddButton");
  clickElement(dbg, "watchExpressionsAddButton");

  await waitFor(() =>
    watchExpressionHeaderEl.getAttribute("aria-expanded", "true")
  );
  ok(true, "The accordion toggle is properly marked as expanded again");

  info("Check that the input element gets focused");
  await waitForElementWithSelector(dbg, ".expression-input-container.focused");
  ok(true, "Found the expression input container, with the focused class");
  is(
    dbg.win.document.activeElement.classList.contains("input-expression"),
    true,
    "The active element is the watch expression input"
  );
});
