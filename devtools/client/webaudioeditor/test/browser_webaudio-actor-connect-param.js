/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the `connect-param` event on the web audio actor.
 */

add_task(async function() {
  let { target, front } = await initBackend(CONNECT_PARAM_URL);
  let [, , [destNode, carrierNode, modNode, gainNode], , connectParam] = await Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    getN(front, "create-node", 4),
    get2(front, "connect-node"),
    once(front, "connect-param")
  ]);

  info(connectParam);

  is(connectParam.source.actorID, modNode.actorID, "`connect-param` has correct actor for `source`");
  is(connectParam.dest.actorID, gainNode.actorID, "`connect-param` has correct actor for `dest`");
  is(connectParam.param, "gain", "`connect-param` has correct parameter name for `param`");

  await removeTab(target.tab);
});
