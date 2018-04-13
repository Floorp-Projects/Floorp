/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view correctly displays all properties for nodes
 * correctly, with default values and correct types.
 */

var MEDIA_PERMISSION = "media.navigator.permission.disabled";

function waitForDeviceClosed() {
  info("Checking that getUserMedia streams are no longer in use.");

  let temp = {};
  ChromeUtils.import("resource:///modules/webrtcUI.jsm", temp);
  let webrtcUI = temp.webrtcUI;

  if (!webrtcUI.showGlobalIndicator) {
    return Promise.resolve();
  }

  return new Promise((resolve, reject) => {
    const message = "webrtc:UpdateGlobalIndicators";
    Services.ppmm.addMessageListener(message, function listener(aMessage) {
      info("Received " + message + " message");
      if (!aMessage.data.showGlobalIndicator) {
        Services.ppmm.removeMessageListener(message, listener);
        resolve();
      }
    });
  });
}

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(MEDIA_NODES_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  let gVars = PropertiesView._propsView;

  // Auto enable getUserMedia
  let mediaPermissionPref = Services.prefs.getBoolPref(MEDIA_PERMISSION);
  Services.prefs.setBoolPref(MEDIA_PERMISSION, true);

  await loadFrameScriptUtils();

  let events = Promise.all([
    getN(gFront, "create-node", 4),
    waitForGraphRendered(panelWin, 4, 0)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIds = actors.map(actor => actor.actorID);

  let types = [
    "AudioDestinationNode", "MediaElementAudioSourceNode",
    "MediaStreamAudioSourceNode", "MediaStreamAudioDestinationNode"
  ];

  let defaults = await Promise.all(types.map(type => nodeDefaultValues(type)));

  for (let i = 0; i < types.length; i++) {
    click(panelWin, findGraphNode(panelWin, nodeIds[i]));
    await waitForInspectorRender(panelWin, EVENTS);
    checkVariableView(gVars, 0, defaults[i], types[i]);
  }

  // Reset permissions on getUserMedia
  Services.prefs.setBoolPref(MEDIA_PERMISSION, mediaPermissionPref);

  await teardown(target);

  await waitForDeviceClosed();
});
