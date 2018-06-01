/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#addAutomationEvent() checking automation values, also using
 * a curve as the last event to check duration spread.
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_CONTEXT_URL);
  const [_, [destNode, oscNode, gainNode]] = await Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  const t0 = 0, t1 = 0.1, t2 = 0.2, t3 = 0.3, t4 = 0.4, t5 = 0.6, t6 = 0.7, t7 = 1;
  const curve = [-1, 0, 1];
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.2, t0]);
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.3, t1]);
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.4, t2]);
  await oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [1, t3]);
  await oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [0.15, t4]);
  await oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.75, t5]);
  await oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.05, t6]);
  // End with a curve here so we can get proper results on the last event (which takes into account
  // duration)
  await oscNode.addAutomationEvent("frequency", "setValueCurveAtTime", [curve, t6, t7 - t6]);

  const { events, values } = await oscNode.getAutomationData("frequency");

  is(events.length, 8, "8 recorded events returned.");
  is(values.length, 2000, "2000 value points returned.");

  checkAutomationValue(values, 0.05, 0.2);
  checkAutomationValue(values, 0.1, 0.3);
  checkAutomationValue(values, 0.15, 0.3);
  checkAutomationValue(values, 0.2, 0.4);
  checkAutomationValue(values, 0.25, 0.7);
  checkAutomationValue(values, 0.3, 1);
  checkAutomationValue(values, 0.35, 0.575);
  checkAutomationValue(values, 0.4, 0.15);
  checkAutomationValue(values, 0.45, 0.15 * Math.pow(0.75 / 0.15, 0.05 / 0.2));
  checkAutomationValue(values, 0.5, 0.15 * Math.pow(0.75 / 0.15, 0.5));
  checkAutomationValue(values, 0.55, 0.15 * Math.pow(0.75 / 0.15, 0.15 / 0.2));
  checkAutomationValue(values, 0.6, 0.75);
  checkAutomationValue(values, 0.65, 0.75 * Math.pow(0.05 / 0.75, 0.5));
  checkAutomationValue(values, 0.705, -1); // Increase this time a bit to prevent off by the previous exponential amount
  checkAutomationValue(values, 0.8, 0);
  checkAutomationValue(values, 0.9, 1);
  checkAutomationValue(values, 1, 1);

  await removeTab(target.tab);
});
