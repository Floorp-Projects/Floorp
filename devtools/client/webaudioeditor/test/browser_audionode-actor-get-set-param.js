/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParam() / AudioNode#setParam()
 */

add_task(function* () {
  let { target, front } = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, [destNode, oscNode, gainNode]] = yield Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  let freq = yield oscNode.getParam("frequency");
  info(typeof freq);
  is(freq, 440, "AudioNode:getParam correctly fetches AudioParam");

  let type = yield oscNode.getParam("type");
  is(type, "sine", "AudioNode:getParam correctly fetches non-AudioParam");

  type = yield oscNode.getParam("not-a-valid-param");
  ok(type.type === "undefined",
    "AudioNode:getParam correctly returns a grip value for `undefined` for an invalid param.");

  let resSuccess = yield oscNode.setParam("frequency", 220);
  freq = yield oscNode.getParam("frequency");
  is(freq, 220, "AudioNode:setParam correctly sets a `number` AudioParam");
  is(resSuccess, undefined, "AudioNode:setParam returns undefined for correctly set AudioParam");

  resSuccess = yield oscNode.setParam("type", "square");
  type = yield oscNode.getParam("type");
  is(type, "square", "AudioNode:setParam correctly sets a `string` non-AudioParam");
  is(resSuccess, undefined, "AudioNode:setParam returns undefined for correctly set AudioParam");

  try {
    yield oscNode.setParam("frequency", "hello");
    ok(false, "setParam with invalid types should throw");
  } catch (e) {
    ok(/is not a finite floating-point/.test(e.message), "AudioNode:setParam returns error with correct message when attempting an invalid assignment");
    is(e.type, "TypeError", "AudioNode:setParam returns error with correct type when attempting an invalid assignment");
    freq = yield oscNode.getParam("frequency");
    is(freq, 220, "AudioNode:setParam does not modify value when an error occurs");
  }

  yield removeTab(target.tab);
});
