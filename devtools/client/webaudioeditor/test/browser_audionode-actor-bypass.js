/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#bypass(), AudioNode#isBypassed()
 */

add_task(function*() {
  let { target, front } = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, [destNode, oscNode, gainNode]] = yield Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node")
  ]);

  is((yield gainNode.isBypassed()), false, "Nodes start off unbypassed.");

  info("Calling node#bypass(true)");
  let isBypassed = yield gainNode.bypass(true);

  is(isBypassed, true, "node.bypass(true) resolves to true");
  is((yield gainNode.isBypassed()), true, "Node is now bypassed.");

  info("Calling node#bypass(false)");
  isBypassed = yield gainNode.bypass(false);

  is(isBypassed, false, "node.bypass(false) resolves to false");
  is((yield gainNode.isBypassed()), false, "Node back to being unbypassed.");

  info("Calling node#bypass(true) on unbypassable node");
  isBypassed = yield destNode.bypass(true);

  is(isBypassed, false, "node.bypass(true) resolves to false for unbypassable node");
  is((yield gainNode.isBypassed()), false, "Unbypassable node is unaffect");

  yield removeTab(target.tab);
});
