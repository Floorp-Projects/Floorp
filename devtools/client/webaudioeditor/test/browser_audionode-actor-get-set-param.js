/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParam() / AudioNode#setParam()
 */

add_task(async function() {
  let { target, front } = await initBackend(SIMPLE_CONTEXT_URL);
  let [_, [destNode, oscNode, gainNode]] = await Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  let freq = await oscNode.getParam("frequency");
  info(typeof freq);
  is(freq, 440, "AudioNode:getParam correctly fetches AudioParam");

  let type = await oscNode.getParam("type");
  is(type, "sine", "AudioNode:getParam correctly fetches non-AudioParam");

  type = await oscNode.getParam("not-a-valid-param");
  ok(type.type === "undefined",
    "AudioNode:getParam correctly returns a grip value for `undefined` for an invalid param.");

  let resSuccess = await oscNode.setParam("frequency", 220);
  freq = await oscNode.getParam("frequency");
  is(freq, 220, "AudioNode:setParam correctly sets a `number` AudioParam");
  is(resSuccess, undefined, "AudioNode:setParam returns undefined for correctly set AudioParam");

  resSuccess = await oscNode.setParam("type", "square");
  type = await oscNode.getParam("type");
  is(type, "square", "AudioNode:setParam correctly sets a `string` non-AudioParam");
  is(resSuccess, undefined, "AudioNode:setParam returns undefined for correctly set AudioParam");

  try {
    await oscNode.setParam("frequency", "hello");
    ok(false, "setParam with invalid types should throw");
  } catch (e) {
    ok(/is not a finite floating-point/.test(e.message), "AudioNode:setParam returns error with correct message when attempting an invalid assignment");
    is(e.type, "TypeError", "AudioNode:setParam returns error with correct type when attempting an invalid assignment");
    freq = await oscNode.getParam("frequency");
    is(freq, 220, "AudioNode:setParam does not modify value when an error occurs");
  }

  await removeTab(target.tab);
});
