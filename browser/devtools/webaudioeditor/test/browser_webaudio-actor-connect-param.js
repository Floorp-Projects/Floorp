/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the `connect-param` event on the web audio actor.
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(CONNECT_PARAM_URL);
  let [_, _, [destNode, carrierNode, modNode, gainNode], _, connectParam] = yield Promise.all([
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

  yield removeTab(target.tab);
  finish();
}
