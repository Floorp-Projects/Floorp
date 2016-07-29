// Some common helpers

function touchActionSetup(testDriver) {
  add_completion_callback(subtestDone);
  document.body.addEventListener('touchend', testDriver, { passive: true });
}

function touchScrollRight(aSelector = '#target0', aX = 20, aY = 20) {
  var target = document.querySelector(aSelector);
  return ok(synthesizeNativeTouchDrag(target, aX + 40, aY, -40, 0), "Synthesized horizontal drag");
}

function touchScrollDown(aSelector = '#target0', aX = 20, aY = 20) {
  var target = document.querySelector(aSelector);
  return ok(synthesizeNativeTouchDrag(target, aX, aY + 40, 0, -40), "Synthesized vertical drag");
}

function tapComplete() {
  var button = document.getElementById('btnComplete');
  return button.click();
}

// The main body functions to simulate the input events required for the named test

function* pointerevent_touch_action_auto_css_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollDown();
}

function* pointerevent_touch_action_button_test_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollDown('#target0 > button');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > button');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_inherit_child_auto_child_none_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_inherit_child_none_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_inherit_child_pan_x_child_pan_x_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_inherit_child_pan_x_child_pan_y_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_inherit_highest_parent_none_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div');
}

function* pointerevent_touch_action_inherit_parent_none_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_none_css_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_pan_x_css_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_pan_x_pan_y_pan_y_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0 > div div');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_pan_x_pan_y_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight();
}

function* pointerevent_touch_action_pan_y_css_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_span_test_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollDown('#testspan');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#testspan');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_svg_test_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollRight();
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollDown('#target0', 250, 250);
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#target0', 250, 250);
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

function* pointerevent_touch_action_table_test_touch_manual(testDriver) {
  touchActionSetup(testDriver);

  yield touchScrollDown('#row1');
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollRight('#row1');
  yield waitForApzFlushedRepaints(testDriver);
  yield setTimeout(testDriver, 2 * scrollReturnInterval);
  yield touchScrollDown('#cell3');
  yield waitForApzFlushedRepaints(testDriver);
  yield touchScrollRight('#cell3');
  yield waitForApzFlushedRepaints(testDriver);
  yield tapComplete();
}

// This the stuff that runs the appropriate body function above

var test = eval(_ACTIVE_TEST_NAME.replace(/-/g, '_'));
waitUntilApzStable().then(runContinuation(test));
