/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#bypass(), AudioNode#isBypassed()
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_CONTEXT_URL);
  const [_, [destNode, oscNode, gainNode]] = await Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  is((await gainNode.isBypassed()), false, "Nodes start off unbypassed.");

  info("Calling node#bypass(true)");
  let isBypassed = await gainNode.bypass(true);

  is(isBypassed, true, "node.bypass(true) resolves to true");
  is((await gainNode.isBypassed()), true, "Node is now bypassed.");

  info("Calling node#bypass(false)");
  isBypassed = await gainNode.bypass(false);

  is(isBypassed, false, "node.bypass(false) resolves to false");
  is((await gainNode.isBypassed()), false, "Node back to being unbypassed.");

  info("Calling node#bypass(true) on unbypassable node");
  isBypassed = await destNode.bypass(true);

  is(isBypassed, false, "node.bypass(true) resolves to false for unbypassable node");
  is((await gainNode.isBypassed()), false, "Unbypassable node is unaffect");

  await removeTab(target.tab);
});
