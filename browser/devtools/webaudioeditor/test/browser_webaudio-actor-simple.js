/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic communication of Web Audio actor
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, __, [destNode, oscNode, gainNode], [connect1, connect2]] = yield Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    get3(front, "create-node"),
    get2(front, "connect-node")
  ]);

  let destType = yield destNode.getType();
  let oscType = yield oscNode.getType();
  let gainType = yield gainNode.getType();

  is(destType, "AudioDestinationNode", "WebAudioActor:create-node returns AudioNodeActor for AudioDestination");
  is(oscType, "OscillatorNode", "WebAudioActor:create-node returns AudioNodeActor");
  is(gainType, "GainNode", "WebAudioActor:create-node returns AudioNodeActor");

  let { source, dest } = connect1;
  is(source.actorID, oscNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on source (osc->gain)");
  is(dest.actorID, gainNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on dest (osc->gain)");

  let { source, dest } = connect2;
  is(source.actorID, gainNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on source (gain->dest)");
  is(dest.actorID, destNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on dest (gain->dest)");

  yield removeTab(target.tab);
  finish();
}
