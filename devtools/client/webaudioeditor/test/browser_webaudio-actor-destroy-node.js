/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test `destroy-node` event on WebAudioActor.
 */

add_task(async function() {
  const { target, front } = await initBackend(DESTROY_NODES_URL);

  const [, , created] = await Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    // Should create dest, gain, and oscillator node and 10
    // disposable buffer nodes
    getN(front, "create-node", 13)
  ]);

  const waitUntilDestroyed = getN(front, "destroy-node", 10);

  // Force CC so we can ensure it's run to clear out dead AudioNodes
  forceNodeCollection();

  const destroyed = await waitUntilDestroyed;

  destroyed.forEach((node, i) => {
    is(node.type, "AudioBufferSourceNode", "Only buffer nodes are destroyed");
    ok(actorIsInList(created, destroyed[i]),
      "`destroy-node` called only on AudioNodes in current document.");
  });

  await removeTab(target.tab);
});

function actorIsInList(list, actor) {
  for (let i = 0; i < list.length; i++) {
    if (list[i].actorID === actor.actorID) {
      return list[i];
    }
  }
  return null;
}
