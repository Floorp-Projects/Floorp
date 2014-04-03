/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#isSource()
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = yield Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 14)
  ]);

  let actualTypes = yield Promise.all(nodes.map(node => node.getType()));
  let isSourceResult = yield Promise.all(nodes.map(node => node.isSource()));

  actualTypes.forEach((type, i) => {
    let shouldBeSource = type === "AudioBufferSourceNode" || type === "OscillatorNode";
    if (shouldBeSource)
      is(isSourceResult[i], true, type + "'s isSource() yields into `true`");
    else
      is(isSourceResult[i], false, type + "'s isSource() yields into `false`");
  });

  yield removeTab(target.tab);
  finish();
}
