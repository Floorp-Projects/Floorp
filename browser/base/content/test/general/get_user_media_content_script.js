/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

const kObservedTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended"
];

var gObservedTopics = {};
function observer(aSubject, aTopic, aData) {
  if (!(aTopic in gObservedTopics))
    gObservedTopics[aTopic] = 1;
  else
    ++gObservedTopics[aTopic];
}

kObservedTopics.forEach(topic => {
  Services.obs.addObserver(observer, topic, false);
});

addMessageListener("Test:ExpectObserverCalled", ({data}) => {
  sendAsyncMessage("Test:ExpectObserverCalled:Reply",
                   {count: gObservedTopics[data]});
  if (data in gObservedTopics)
    --gObservedTopics[data];
});

addMessageListener("Test:TodoObserverNotCalled", ({data}) => {
  sendAsyncMessage("Test:TodoObserverNotCalled:Reply",
                   {count: gObservedTopics[data]});
  if (gObservedTopics[data] == 1)
    gObservedTopics[data] = 0;
});

addMessageListener("Test:ExpectNoObserverCalled", data => {
  sendAsyncMessage("Test:ExpectNoObserverCalled:Reply", gObservedTopics);
  gObservedTopics = {};
});

function _getMediaCaptureState() {
  let hasVideo = {};
  let hasAudio = {};
  let hasScreenShare = {};
  let hasWindowShare = {};
  MediaManagerService.mediaCaptureWindowState(content, hasVideo, hasAudio,
                                              hasScreenShare, hasWindowShare);
  if (hasVideo.value && hasAudio.value)
    return "CameraAndMicrophone";
  if (hasVideo.value)
    return "Camera";
  if (hasAudio.value)
    return "Microphone";
  if (hasScreenShare.value)
    return "Screen";
  if (hasWindowShare.value)
    return "Window";
  return "none";
}

addMessageListener("Test:GetMediaCaptureState", data => {
  sendAsyncMessage("Test:MediaCaptureState", _getMediaCaptureState());
});

addMessageListener("Test:WaitForObserverCall", ({data}) => {
  let topic = data;
  Services.obs.addObserver(function observer() {
    sendAsyncMessage("Test:ObserverCalled", topic);
    Services.obs.removeObserver(observer, topic);

    if (kObservedTopics.indexOf(topic) != -1) {
      if (!(topic in gObservedTopics))
        gObservedTopics[topic] = -1;
      else
        --gObservedTopics[topic];
    }
  }, topic, false);
});
