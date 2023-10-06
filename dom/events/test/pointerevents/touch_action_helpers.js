// Some common helpers

function promiseTimeout(delay) {
  return new Promise(resolve => {
    setTimeout(resolve, delay);
  });
}

function promiseTouchStart(element) {
  return new Promise(resolve => {
    element.addEventListener("touchstart", resolve, {
      passive: true,
      once: true,
    });
  });
}

async function touchScrollRight(aSelector = "#target0", aX = 20, aY = 20) {
  const target = document.querySelector(aSelector);
  const touchStartPromise = promiseTouchStart(document.body);
  const touchEndPromise = promiseTouchEnd(document.body);
  dump("Synthesizing horizontal drag\n");
  await promiseNativePointerDrag(target, "touch", aX + 40, aY, -40, 0);
  await touchStartPromise;
  dump("Got touchstart from the horizontal drag\n");
  await touchEndPromise;
  dump("Got touchend from the horizontal drag\n");
}

async function touchScrollDown(aSelector = "#target0", aX = 20, aY = 20) {
  const target = document.querySelector(aSelector);
  const touchStartPromise = promiseTouchStart(document.body);
  const touchEndPromise = promiseTouchEnd(document.body);
  dump("Synthesizing vertical drag\n");
  await promiseNativePointerDrag(target, "touch", aX, aY + 40, 0, -40);
  await touchStartPromise;
  dump("Got touchstart from the vertical drag\n");
  await touchEndPromise;
  dump("Got touchend from the vertical drag\n");
}

async function tapCompleteAndWaitTestDone() {
  let testDone = new Promise(resolve => {
    add_completion_callback(resolve);
  });

  var button = document.getElementById("btnComplete");
  button.click();
  await testDone;
}

function promiseResetScrollLeft(aSelector = "#target0") {
  var target = document.querySelector(aSelector);
  return new Promise(resolve => {
    target.addEventListener("scroll", function onScroll() {
      if (target.scrollLeft == 0) {
        target.removeEventListener("scroll", onScroll);
        resolve();
      }
    });
  });
}

// The main body functions to simulate the input events required for the named test

async function pointerevent_touch_action_auto_css_touch_manual() {
  let testDone = new Promise(resolve => {
    add_completion_callback(resolve);
  });

  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await touchScrollDown();
  await testDone;
}

async function pointerevent_touch_action_button_test_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  await touchScrollRight();
  let resetScrollLeftPromise = promiseResetScrollLeft();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  // Wait for resetting target0's scrollLeft to avoid the reset break the
  // following scroll behaviors.
  await resetScrollLeftPromise;
  await touchScrollDown("#target0 > button");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > button");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_inherit_child_auto_child_none_touch_manual() {
  await touchScrollDown("#target0 > div div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div div");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_inherit_child_none_touch_manual() {
  await touchScrollDown("#target0 > div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_inherit_child_pan_x_child_pan_x_touch_manual() {
  await touchScrollDown("#target0 > div div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div div");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_inherit_child_pan_x_child_pan_y_touch_manual() {
  await touchScrollDown("#target0 > div div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div div");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_inherit_highest_parent_none_touch_manual() {
  let testDone = new Promise(resolve => {
    add_completion_callback(resolve);
  });

  await touchScrollDown("#target0 > div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div");
  await testDone;
}

async function pointerevent_touch_action_inherit_parent_none_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_none_css_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_pan_x_css_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_pan_x_pan_y_pan_y_touch_manual() {
  await touchScrollDown("#target0 > div div");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0 > div div");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_pan_x_pan_y_touch_manual() {
  let testDone = new Promise(resolve => {
    add_completion_callback(resolve);
  });

  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await touchScrollRight();
  await testDone;
}

async function pointerevent_touch_action_pan_y_css_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_span_test_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  await touchScrollRight();
  let resetScrollLeftPromise = promiseResetScrollLeft();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  // Wait for resetting target0's scrollLeft to avoid the reset break the
  // following scroll behaviors.
  await resetScrollLeftPromise;
  await touchScrollDown("#testspan");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#testspan");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_svg_test_touch_manual() {
  await touchScrollDown();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  await touchScrollRight();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  await touchScrollDown("#target0", 250, 250);
  await promiseApzFlushedRepaints();
  await touchScrollRight("#target0", 250, 250);
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

async function pointerevent_touch_action_table_test_touch_manual() {
  await touchScrollDown("#row1");
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  await touchScrollRight("#row1");
  let resetScrollLeftPromise = promiseResetScrollLeft();
  await promiseApzFlushedRepaints();
  await promiseTimeout(2 * scrollReturnInterval);
  // Wait for resetting target0's scrollLeft to avoid the reset break the
  // following scroll behaviors.
  await resetScrollLeftPromise;
  await touchScrollDown("#cell3");
  await promiseApzFlushedRepaints();
  await touchScrollRight("#cell3");
  await promiseApzFlushedRepaints();
  await tapCompleteAndWaitTestDone();
}

// This the stuff that runs the appropriate body function above

// eslint-disable-next-line no-eval
var test = eval(_ACTIVE_TEST_NAME.replace(/-/g, "_"));
waitUntilApzStable().then(test).then(subtestDone, subtestFailed);
