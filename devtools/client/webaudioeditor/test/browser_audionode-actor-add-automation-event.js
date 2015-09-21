/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#addAutomationEvent();
 */

add_task(function*() {
  let { target, front } = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, [destNode, oscNode, gainNode]] = yield Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);
  let count = 0;
  let counter = () => count++;
  front.on("automation-event", counter);

  let t0 = 0, t1 = 0.1, t2 = 0.2, t3 = 0.3, t4 = 0.4, t5 = 0.6, t6 = 0.7, t7 = 1;
  let curve = [-1, 0, 1];
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.2, t0]);
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.3, t1]);
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.4, t2]);
  yield oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [1, t3]);
  yield oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [0.15, t4]);
  yield oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.75, t5]);
  yield oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.5, t6]);
  yield oscNode.addAutomationEvent("frequency", "setValueCurveAtTime", [curve, t7, t7 - t6]);
  yield oscNode.addAutomationEvent("frequency", "setTargetAtTime", [20, 2, 5]);

  ok(true, "successfully set automation events for valid automation events");

  try {
    yield oscNode.addAutomationEvent("frequency", "notAMethod", 20, 2, 5);
    ok(false, "non-automation methods should not be successful");
  } catch (e) {
    ok(/invalid/.test(e.message), "AudioNode:addAutomationEvent fails for invalid automation methods");
  }

  try {
    yield oscNode.addAutomationEvent("invalidparam", "setValueAtTime", 0.2, t0);
    ok(false, "automating non-AudioParams should not be successful");
  } catch (e) {
    ok(/invalid/.test(e.message), "AudioNode:addAutomationEvent fails for a non AudioParam");
  }

  front.off("automation-event", counter);

  is(count, 9,
    "when calling `addAutomationEvent`, the WebAudioActor should still fire `automation-event`.");

  yield removeTab(target.tab);
});
