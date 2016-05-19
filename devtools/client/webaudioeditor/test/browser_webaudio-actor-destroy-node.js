/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test `destroy-node` event on WebAudioActor.
 */

add_task(function* () {
  let { target, front } = yield initBackend(DESTROY_NODES_URL);

  let waitUntilDestroyed = getN(front, "destroy-node", 10);
  let [, , created] = yield Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    // Should create 1 destination node and 10 disposable buffer nodes
    getN(front, "create-node", 13)
  ]);

  // Wait for a tick before gc to prevent this test from intermittent timeout
  // where the node never get collected.
  yield DevToolsUtils.waitForTick();

  // Force CC so we can ensure it's run to clear out dead AudioNodes
  forceCC();

  let destroyed = yield waitUntilDestroyed;

  destroyed.forEach((node, i) => {
    ok(node.type, "AudioBufferSourceNode", "Only buffer nodes are destroyed");
    ok(actorIsInList(created, destroyed[i]),
      "`destroy-node` called only on AudioNodes in current document.");
  });

  yield removeTab(target.tab);
});

function actorIsInList(list, actor) {
  for (let i = 0; i < list.length; i++) {
    if (list[i].actorID === actor.actorID)
      return list[i];
  }
  return null;
}
