/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// checks to see if the frame is selected and the title is correct
function isFrameSelected(dbg, index, title) {
  const $frame = findElement(dbg, "frame", index);
  const frame = dbg.selectors.getSelectedFrame(dbg.getState());

  const elSelected = $frame.classList.contains("selected");
  const titleSelected = frame.displayName == title;

  return elSelected && titleSelected;
}

function toggleButton(dbg) {
  const callStackBody = findElement(dbg, "callStackBody");
  return callStackBody.querySelector(".show-more");
}

add_task(function* () {
  const dbg = yield initDebugger("doc-script-switching.html");

  toggleCallStack(dbg);

  const notPaused = findElement(dbg, "callStackBody").innerText;
  is(notPaused, "Not Paused", "Not paused message is shown");

  invokeInTab("firstCall");
  yield waitForPaused(dbg);

  ok(isFrameSelected(dbg, 1, "secondCall"), "the first frame is selected");

  let button = toggleButton(dbg);
  ok(!button, "toggle button shouldn't be there");
});

add_task(function* () {
  const dbg = yield initDebugger("doc-frames.html");

  toggleCallStack(dbg);

  invokeInTab("startRecursion");
  yield waitForPaused(dbg);

  ok(isFrameSelected(dbg, 1, "recurseA"), "the first frame is selected");

  // check to make sure that the toggle button isn't there
  let button = toggleButton(dbg);
  let frames = findAllElements(dbg, "frames");
  is(button.innerText, "Expand Rows", "toggle button should be expand");
  is(frames.length, 7, "There should be at most seven frames");

  button.click();

  button = toggleButton(dbg);
  frames = findAllElements(dbg, "frames");
  is(button.innerText, "Collapse Rows", "toggle button should be collapsed");
  is(frames.length, 22, "All of the frames should be shown");
});
