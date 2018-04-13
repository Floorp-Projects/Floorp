/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the WebAudioActor receives and emits the `automation-event` events
 * with correct arguments from the content.
 */

add_task(async function() {
  let { target, front } = await initBackend(AUTOMATION_URL);
  let events = [];

  let expected = [
    ["setValueAtTime", 0.2, 0],
    ["linearRampToValueAtTime", 1, 0.3],
    ["exponentialRampToValueAtTime", 0.75, 0.6],
    ["setValueCurveAtTime", [-1, 0, 1], 0.7, 0.3],
  ];

  front.on("automation-event", onAutomationEvent);

  let [_, __, [destNode, oscNode, gainNode], [connect1, connect2]] = await Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    get3(front, "create-node"),
    get2(front, "connect-node")
  ]);

  is(events.length, 4, "correct number of events fired");

  function onAutomationEvent(e) {
    let { eventName, paramName, args } = e;
    let exp = expected[events.length];

    is(eventName, exp[0], "correct eventName in event");
    is(paramName, "frequency", "correct paramName in event");
    is(args.length, exp.length - 1, "correct length in args");

    args.forEach((a, i) => {
      // In the case of an array
      if (typeof a === "object") {
        a.forEach((f, j) => is(f, exp[i + 1][j], `correct argument in Float32Array: ${f}`));
      } else {
        is(a, exp[i + 1], `correct ${i + 1}th argument in args: ${a}`);
      }
    });
    events.push([eventName].concat(args));
  }

  front.off("automation-event", onAutomationEvent);
  await removeTab(target.tab);
});
