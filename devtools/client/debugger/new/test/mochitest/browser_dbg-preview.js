/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getCoordsFromPosition(cm, { line, ch }) {
  return cm.charCoords({ line: ~~line, ch: ~~ch });
}

function hoverAtPos(dbg, { line, ch }) {
  const cm = getCM(dbg);
  const coords = getCoordsFromPosition(cm, { line: line - 1, ch });
  const tokenEl = dbg.win.document.elementFromPoint(coords.left, coords.top);
  tokenEl.dispatchEvent(
    new MouseEvent("mouseover", {
      bubbles: true,
      cancelable: true,
      view: dbg.win
    })
  );
}

function assertTooltip(dbg, { result, expression }) {
  const previewEl = findElement(dbg, "tooltip");
  is(previewEl.innerText, result, "Preview text shown to user");

  const preview = dbg.selectors.getPreview(dbg.getState());
  is(`${preview.result}`, result, "Preview.result");
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

function assertPopup(dbg, { field, value, expression }) {
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
  const dbg = await initDebugger("doc-scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;
  const simple3 = findSource(dbg, "simple3.js");

  await selectSource(dbg, "simple3");

  await addBreakpoint(dbg, simple3, 5);

  invokeInTab("simple");
  await waitForPaused(dbg);

  const tooltipPreviewed = waitForDispatch(dbg, "SET_PREVIEW");
  hoverAtPos(dbg, { line: 5, ch: 12 });
  await tooltipPreviewed;
  assertTooltip(dbg, { result: "3", expression: "result" });

  const popupPreviewed = waitForDispatch(dbg, "SET_PREVIEW");
  hoverAtPos(dbg, { line: 2, ch: 10 });
  await popupPreviewed;
  assertPopup(dbg, { field: "foo", value: "1", expression: "obj" });
  assertPopup(dbg, { field: "bar", value: "2", expression: "obj" });
});
