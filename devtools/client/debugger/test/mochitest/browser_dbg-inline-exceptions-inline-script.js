/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks the appearance of an inline exception in inline script
// and the content of the exception tooltip.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-exceptions-inline-script.html");
  await selectSource(dbg, "doc-exceptions-inline-script.html");

  info("Hovers over the inline exception mark text.");
  await assertInlineExceptionPreview(dbg, 7, 39, {
    fields: [
      [
        "<anonymous>",
        "https://example.com/browser/devtools/client/debugger/test/mochitest/examples/doc-exceptions-inline-script.html:7",
      ],
    ],
    result: "TypeError: [].plop is not a function",
    expression: "plop",
  });

  const excLineEls = findAllElementsWithSelector(dbg, ".line-exception");
  const excTextMarkEls = findAllElementsWithSelector(
    dbg,
    ".mark-text-exception"
  );

  is(excLineEls.length, 1, "The editor has one exception line");
  is(excTextMarkEls.length, 1, "One token is marked as an exception.");
});
