/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#source
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_NODES_URL);
  const [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 14)
  ]);

  const actualTypes = nodes.map(node => node.type);
  const isSourceResult = nodes.map(node => node.source);

  actualTypes.forEach((type, i) => {
    const shouldBeSource = type === "AudioBufferSourceNode" || type === "OscillatorNode";
    if (shouldBeSource) {
      is(isSourceResult[i], true, type + "'s `source` is `true`");
    } else {
      is(isSourceResult[i], false, type + "'s `source` is `false`");
    }
  });

  await removeTab(target.tab);
});
