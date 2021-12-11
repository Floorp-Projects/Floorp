/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks the appearance of an inline exception
// and the content of the exception tooltip.

add_task(async function() {
  const dbg = await initDebugger("doc-exceptions.html");
  await selectSource(dbg, "exceptions.js");

  info("Hovers over the inline exception mark text.");
  await assertPreviewTextValue(dbg, 81, 10, { text: 'TypeError: "abc".push is not a function' });
  await closePreviewAtPos(dbg, 81, 10);

  const excLineEls = findAllElementsWithSelector(dbg, ".line-exception");
  const excTextMarkEls = findAllElementsWithSelector(dbg, ".mark-text-exception");

  is(excLineEls.length, 1, "The editor has one exception line");
  is(excTextMarkEls.length, 1, "One token is marked as an exception.");
});

