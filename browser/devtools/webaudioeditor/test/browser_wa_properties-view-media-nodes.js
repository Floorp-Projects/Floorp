/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view correctly displays all properties for nodes
 * correctly, with default values and correct types.
 */

let MEDIA_PERMISSION = "media.navigator.permission.disabled";

function waitForDeviceClosed() {
  info("Checking that getUserMedia streams are no longer in use.");

  let temp = {};
  Cu.import("resource:///modules/webrtcUI.jsm", temp);
  let webrtcUI = temp.webrtcUI;

  if (!webrtcUI.showGlobalIndicator)
    return Promise.resolve();

  let deferred = Promise.defer();

  const message = "webrtc:UpdateGlobalIndicators";
  let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageBroadcaster);
  ppmm.addMessageListener(message, function listener(aMessage) {
    info("Received " + message + " message");
    if (!aMessage.data.showGlobalIndicator) {
      ppmm.removeMessageListener(message, listener);
      deferred.resolve();
    }
  });

  return deferred.promise;
}

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(MEDIA_NODES_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, WebAudioInspectorView } = panelWin;
  let gVars = WebAudioInspectorView._propsView;

  // Auto enable getUserMedia
  let mediaPermissionPref = Services.prefs.getBoolPref(MEDIA_PERMISSION);
  Services.prefs.setBoolPref(MEDIA_PERMISSION, true);

  reload(target);

  let [actors] = yield Promise.all([
    getN(gFront, "create-node", 4),
    waitForGraphRendered(panelWin, 4, 0)
  ]);

  let nodeIds = actors.map(actor => actor.actorID);
  let types = [
    "AudioDestinationNode", "MediaElementAudioSourceNode",
    "MediaStreamAudioSourceNode", "MediaStreamAudioDestinationNode"
  ];

  for (let i = 0; i < types.length; i++) {
    click(panelWin, findGraphNode(panelWin, nodeIds[i]));
    yield once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET);
    checkVariableView(gVars, 0, NODE_DEFAULT_VALUES[types[i]], types[i]);
  }

  // Reset permissions on getUserMedia
  Services.prefs.setBoolPref(MEDIA_PERMISSION, mediaPermissionPref);

  yield teardown(panel);

  yield waitForDeviceClosed();

  finish();
}
