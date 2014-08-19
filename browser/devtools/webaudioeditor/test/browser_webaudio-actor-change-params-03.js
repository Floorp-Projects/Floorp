/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test WebAudioActor `change-param` events on special types.
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(CHANGE_PARAM_URL);
  let [_, nodes] = yield Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 3)
  ]);

  let shaper = nodes[2];
  let eventCount = 0;

  yield front.enableChangeParamEvents(shaper, 20);

  let onChange = once(front, "change-param");

  shaper.setParam("curve", null);

  let { newValue, oldValue } = yield onChange;

  is(oldValue.type, "object", "`oldValue` should be an object.");
  is(oldValue.class, "Float32Array", "`oldValue` should be of class Float32Array.");
  is(newValue.type, "null", "`newValue` should be null.");

  yield removeTab(target.tab);
  finish();
}
