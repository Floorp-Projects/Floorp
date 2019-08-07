/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function checkInlinePreview(dbg, fnName, inlinePreviews) {
  invokeInTab(fnName);

  await waitForAllElements(dbg, "inlinePreviewWidget", inlinePreviews.length);

  const widgets = findAllElements(dbg, "inlinePreviewWidget");

  inlinePreviews.forEach((inlinePreview, index) => {
    const { identifier, value, expandedValue } = inlinePreview;
    is(
      widgets[index].innerText,
      `${identifier}${value}`,
      `${identifier} in ${fnName} has correct preview`
    );
  });

  await resume(dbg);
}

// Test checking inline preview feature
add_task(async function() {
  await pushPref("devtools.debugger.features.inline-preview", true);

  const dbg = await initDebugger("doc-preview.html", "preview.js");
  await selectSource(dbg, "preview.js");

  await checkInlinePreview(dbg, "empties", [
    { identifier: "a", value: '""' },
    { identifier: "b", value: "false" },
    { identifier: "c", value: "undefined" },
    { identifier: "d", value: "null" },
  ]);

  await checkInlinePreview(dbg, "smalls", [
    { identifier: "a", value: '"..."' },
    { identifier: "b", value: "true" },
    { identifier: "c", value: "1" },
    { identifier: "d", value: "[]" },
    { identifier: "e", value: "Object { }" },
  ]);

  await checkInlinePreview(dbg, "objects", [
    { identifier: "obj", value: "Object { foo: 1 }" },
  ]);

  await checkInlinePreview(dbg, "largeArray", [
    {
      identifier: "bs",
      value: "[ {…}, {…}, {…}, … ]",
    },
  ]);
});
