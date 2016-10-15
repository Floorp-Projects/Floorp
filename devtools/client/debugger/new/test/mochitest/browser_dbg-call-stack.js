/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
function findFrame(dbg, index) {
  return findElement(dbg, "frame", index);
}

// checks to see if the frame is selected and the title is correct
function isFrameSelected(dbg, index, title) {
  const $frame = findFrame(dbg, index);
  const frame = dbg.selectors.getSelectedFrame(dbg.getState());

  const elSelected = $frame.classList.contains("selected");
  const titleSelected = frame.displayName == title;

  return elSelected && titleSelected;
}

add_task(function* () {
  const dbg = yield initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  toggleCallStack(dbg);

  invokeInTab("firstCall");
  yield waitForPaused(dbg);

  ok(isFrameSelected(dbg, 1, "secondCall"), "the first frame is selected");

  findFrame(dbg, 2).click();
  ok(isFrameSelected(dbg, 2, "firstCall"), "the second frame is selected");
});
