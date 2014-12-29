/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#addAutomationEvent() when automation series ends with
 * `setTargetAtTime`, which approaches its target to infinity.
 */

add_task(function*() {
  let { target, front } = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, [destNode, oscNode, gainNode]] = yield Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  let t0 = 0, t1 = 0.1, t2 = 0.2, t3 = 0.3, t4 = 0.4, t5 = 0.6, t6 = 0.7, t7 = 1;
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.2, t0]);
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.3, t1]);
  yield oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.4, t2]);
  yield oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [1, t3]);
  yield oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [0.15, t4]);
  yield oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.75, t5]);
  yield oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.05, t6]);
  // End with a setTargetAtTime event, as the target approaches infinity, which will
  // give us more points to render than the default 2000
  yield oscNode.addAutomationEvent("frequency", "setTargetAtTime", [1, t7, 0.5]);

  let { events, values } = yield oscNode.getAutomationData("frequency");

  is(events.length, 8, "8 recorded events returned.");
  is(values.length, 4000, "6000 value points returned when ending with exponentiall approaching automator.");

  checkAutomationValue(values, 1, 0.05);
  checkAutomationValue(values, 2, 0.87);
  checkAutomationValue(values, 3, 0.98);

  yield removeTab(target.tab);
});
