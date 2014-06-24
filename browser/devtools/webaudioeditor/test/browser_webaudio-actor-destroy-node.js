/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test `destroy-node` event on WebAudioActor.
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(DESTROY_NODES_URL);

  let waitUntilDestroyed = getN(front, "destroy-node", 10);
  let [_, _, created] = yield Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    // Should create 1 destination node and 10 disposable buffer nodes
    getN(front, "create-node", 13)
  ]);

  // Force CC so we can ensure it's run to clear out dead AudioNodes
  forceCC();

  let destroyed = yield waitUntilDestroyed;

  let destroyedTypes = yield Promise.all(destroyed.map(actor => actor.getType()));
  destroyedTypes.forEach((type, i) => {
    ok(type, "AudioBufferSourceNode", "Only buffer nodes are destroyed");
    ok(actorIsInList(created, destroyed[i]),
      "`destroy-node` called only on AudioNodes in current document.");
  });

  yield removeTab(target.tab);
  finish();
}

function actorIsInList (list, actor) {
  for (let i = 0; i < list.length; i++) {
    if (list[i].actorID === actor.actorID)
      return list[i];
  }
  return null;
}
