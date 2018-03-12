/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic communication of Web Audio actor
 */

add_task(function* () {
  let { target, front } = yield initBackend(SIMPLE_CONTEXT_URL);
  let [_, __, [destNode, oscNode, gainNode], [connect1, connect2]] = yield Promise.all([
    front.setup({ reload: true }),
    once(front, "start-context"),
    get3(front, "create-node"),
    get2(front, "connect-node")
  ]);

  is(destNode.type, "AudioDestinationNode", "WebAudioActor:create-node returns AudioNodeActor for AudioDestination");
  is(oscNode.type, "OscillatorNode", "WebAudioActor:create-node returns AudioNodeActor");
  is(gainNode.type, "GainNode", "WebAudioActor:create-node returns AudioNodeActor");

  let { source, dest } = connect1;
  is(source.actorID, oscNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on source (osc->gain)");
  is(dest.actorID, gainNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on dest (osc->gain)");

  ({ source, dest } = connect2);
  is(source.actorID, gainNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on source (gain->dest)");
  is(dest.actorID, destNode.actorID, "WebAudioActor:connect-node returns correct actor with ID on dest (gain->dest)");

  yield removeTab(target.tab);
});
