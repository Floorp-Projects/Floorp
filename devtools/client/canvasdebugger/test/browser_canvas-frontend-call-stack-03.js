/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the a function call's stack can be shown/hidden by double-clicking
 * on a function call item.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_DEEP_STACK_URL);
  const { window, $, $all, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  await reload(target);

  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  const callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  await Promise.all([recordingFinished, callListPopulated]);

  const callItem = CallsListView.getItemAtIndex(2);
  const view = $(".call-item-view", callItem.target);
  const contents = $(".call-item-contents", callItem.target);

  is(view.hasAttribute("call-stack-populated"), false,
    "The call item's view should not have the stack populated yet.");
  is(view.hasAttribute("call-stack-expanded"), false,
    "The call item's view should not have the stack populated yet.");
  is($(".call-item-stack", callItem.target), null,
    "There should be no stack container available yet for the draw call.");

  const callStackDisplayed = once(window, EVENTS.CALL_STACK_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "dblclick" }, contents, window);
  await callStackDisplayed;

  is(view.hasAttribute("call-stack-populated"), true,
    "The call item's view should have the stack populated now.");
  is(view.getAttribute("call-stack-expanded"), "true",
    "The call item's view should have the stack expanded now.");
  isnot($(".call-item-stack", callItem.target), null,
    "There should be a stack container available now for the draw call.");
  is($(".call-item-stack", callItem.target).hidden, false,
    "The stack container should now be visible.");
  // We may have more than 4 functions, depending on whether async
  // stacks are available.
  ok($all(".call-item-stack-fn", callItem.target).length >= 4,
     "There should be at least 4 functions on the stack for the draw call.");

  EventUtils.sendMouseEvent({ type: "dblclick" }, contents, window);

  is(view.hasAttribute("call-stack-populated"), true,
    "The call item's view should still have the stack populated.");
  is(view.getAttribute("call-stack-expanded"), "false",
    "The call item's view should not have the stack expanded anymore.");
  isnot($(".call-item-stack", callItem.target), null,
    "There should still be a stack container available for the draw call.");
  is($(".call-item-stack", callItem.target).hidden, true,
    "The stack container should now be hidden.");
  // We may have more than 4 functions, depending on whether async
  // stacks are available.
  ok($all(".call-item-stack-fn", callItem.target).length >= 4,
     "There should still be at least 4 functions on the stack for the draw call.");

  await teardown(panel);
  finish();
}
