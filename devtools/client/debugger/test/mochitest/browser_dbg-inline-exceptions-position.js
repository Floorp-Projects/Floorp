/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks the appearance of an inline exception
// and the content of the exception tooltip.

"use strict";

function waitForElementsWithSelector(dbg, selector) {
  return waitFor(() => {
    const elems = findAllElementsWithSelector(dbg, selector);
    if (!elems.length) {
      return false;
    }
    return elems;
  });
}

add_task(async function () {
  const dbg = await initDebugger("doc-exception-position.html");

  await selectSource(dbg, "exception-position-1.js");
  let elems = await waitForElementsWithSelector(dbg, ".mark-text-exception");
  is(elems.length, 1);
  is(elems[0].textContent, "a1");

  await selectSource(dbg, "exception-position-2.js");
  elems = await waitForElementsWithSelector(dbg, ".mark-text-exception");
  is(elems.length, 1);
  is(elems[0].textContent, "a2");

  await selectSource(dbg, "exception-position-3.js");
  elems = await waitForElementsWithSelector(dbg, ".mark-text-exception");
  is(elems.length, 1);
  is(elems[0].textContent, "a3");

  await selectSource(dbg, "exception-position-4.js");
  elems = await waitForElementsWithSelector(dbg, ".mark-text-exception");
  is(elems.length, 1);
  is(elems[0].textContent, "a4");
});
