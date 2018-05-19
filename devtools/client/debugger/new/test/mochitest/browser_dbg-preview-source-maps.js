/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function assertNoTooltip(dbg) {
  await waitForTime(200);
  const el = findElement(dbg, "tooltip");
  is(el, null, "Tooltip should not exist");
}

function assertPreviewTooltip(dbg, { result, expression }) {
  const previewEl = findElement(dbg, "tooltip");
  is(previewEl.innerText, result, "Preview text shown to user");

  const preview = dbg.selectors.getPreview(dbg.getState());
  is(`${preview.result}`, result, "Preview.result");
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

function assertPreviewPopup(dbg, { field, value, expression }) {
  const previewEl = findElement(dbg, "popup");
  is(previewEl.innerText, "", "Preview text shown to user");

  const preview = dbg.selectors.getPreview(dbg.getState());

  is(
    `${preview.result.preview.ownProperties[field].value}`,
    value,
    "Preview.result"
  );
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

add_task(async function() {
  const dbg = await initDebugger("doc-sourcemaps.html");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  await waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  await selectSource(dbg, "times2");
  await addBreakpoint(dbg, "times2", 2);

  invokeInTab("keepMeAlive");
  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "times2");

  info(`Test previewing in the original location`);
  await assertPreviews(dbg, [
    { line: 2, column: 10, result: 4, expression: "x" }
  ]);

  info(`Test previewing in the generated location`);
  await dbg.actions.jumpToMappedSelectedLocation();
  await waitForSelectedSource(dbg, "bundle.js");
  await assertPreviews(dbg, [
    { line: 70, column: 11, result: 4, expression: "x" }
  ]);

  info(`Test that you can not preview in another original file`);
  await selectSource(dbg, "output");
  await hoverAtPos(dbg, { line: 2, ch: 16 });
  await assertNoTooltip(dbg);
});
