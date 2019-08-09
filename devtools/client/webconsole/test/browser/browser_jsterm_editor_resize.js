/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the editor can be resized and that its width is persisted.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console test for editor resize";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);

  // Reset editorWidth pref so we have steady results when running multiple times.
  await pushPref("devtools.webconsole.input.editorWidth", null);

  let hud = await openNewTabAndConsole(TEST_URI);
  const getEditorEl = () =>
    hud.ui.outputNode.querySelector(".jsterm-input-container");
  const resizerEl = hud.ui.outputNode.querySelector(".editor-resizer");

  const editorBoundingRect = getEditorEl().getBoundingClientRect();
  const delta = 100;
  const originalWidth = editorBoundingRect.width;
  const clientX = editorBoundingRect.right + delta;
  await resize(resizerEl, clientX);

  const newWidth = Math.floor(originalWidth + delta);
  is(
    Math.floor(getEditorEl().getBoundingClientRect().width),
    newWidth,
    "The editor element was resized as expected"
  );
  info("Close and re-open the console to check if editor width was persisted");
  await closeConsole();
  hud = await openConsole();

  is(
    Math.floor(getEditorEl().getBoundingClientRect().width),
    newWidth,
    "The editor element width was persisted"
  );
  await toggleLayout(hud);

  ok(!getEditorEl().style.width, "The width isn't applied in in-line layout");

  await toggleLayout(hud);
  is(
    getEditorEl().style.width,
    `${newWidth}px`,
    "The width is applied again when switching back to editor"
  );
});

async function resize(resizer, clientX) {
  const doc = resizer.ownerDocument;
  const win = doc.defaultView;

  info("Mouse down to start dragging");
  EventUtils.synthesizeMouseAtCenter(
    resizer,
    { button: 0, type: "mousedown" },
    win
  );
  await waitFor(() => doc.querySelector(".dragging"));

  const event = new MouseEvent("mousemove", { clientX });
  resizer.ownerDocument.dispatchEvent(event);

  info("Mouse up to stop resizing");
  EventUtils.synthesizeMouseAtCenter(
    doc.body,
    { button: 0, type: "mouseup" },
    win
  );

  await waitFor(() => !doc.querySelector(".dragging"));
}
