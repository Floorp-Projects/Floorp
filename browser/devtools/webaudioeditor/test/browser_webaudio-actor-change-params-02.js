/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that listening to param change polling does not break when the AudioNode is collected.
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(DESTROY_NODES_URL);
  let waitUntilDestroyed = getN(front, "destroy-node", 10);
  let [_, nodes] = yield Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 13)
  ]);

  let bufferNode = nodes[6];

  yield front.enableChangeParamEvents(bufferNode, 20);

  front.on("change-param", onChangeParam);

  forceCC();

  yield waitUntilDestroyed;
  yield wait(50);

  front.off("change-param", onChangeParam);

  ok(true, "listening to `change-param` on a dead node doesn't throw.");
  yield removeTab(target.tab);
  finish();

  function onChangeParam (args) {
    ok(false, "`change-param` should not be emitted on a node that hasn't changed params or is dead.");
  }
}
