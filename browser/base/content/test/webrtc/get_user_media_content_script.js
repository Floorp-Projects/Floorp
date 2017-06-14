/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

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

function ignoreEvent(aSubject, aTopic, aData) {
  // With e10s disabled, our content script receives notifications for the
  // preview displayed in our screen sharing permission prompt; ignore them.
  const kBrowserURL = "chrome://browser/content/browser.xul";
  const nsIPropertyBag = Components.interfaces.nsIPropertyBag;
  if (aTopic == "recording-device-events" &&
      aSubject.QueryInterface(nsIPropertyBag).getProperty("requestURL") == kBrowserURL) {
    return true;
  }
  if (aTopic == "recording-window-ended") {
    let win = Services.wm.getOuterWindowWithId(aData).top;
    if (win.document.documentURI == kBrowserURL)
      return true;
  }
  return false;
}

function observer(aSubject, aTopic, aData) {
  if (ignoreEvent(aSubject, aTopic, aData)) {
    return;
  }

  if (!(aTopic in gObservedTopics))
    gObservedTopics[aTopic] = 1;
  else
    ++gObservedTopics[aTopic];
}

kObservedTopics.forEach(topic => {
  Services.obs.addObserver(observer, topic);
});

addMessageListener("Test:ExpectObserverCalled", ({ data: { topic, count } }) => {
  sendAsyncMessage("Test:ExpectObserverCalled:Reply",
                   {count: gObservedTopics[topic]});
  if (topic in gObservedTopics) {
    gObservedTopics[topic] -= count;
  }
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
  let hasAppShare = {};
  let hasBrowserShare = {};
  MediaManagerService.mediaCaptureWindowState(content, hasVideo, hasAudio,
                                              hasScreenShare, hasWindowShare,
                                              hasAppShare, hasBrowserShare);
  let result = {};

  if (hasVideo.value)
    result.video = true;
  if (hasAudio.value)
    result.audio = true;

  if (hasScreenShare.value)
    result.screen = "Screen";
  else if (hasWindowShare.value)
    result.screen = "Window";
  else if (hasAppShare.value)
    result.screen = "Application";
  else if (hasBrowserShare.value)
    result.screen = "Browser";

  return result;
}

addMessageListener("Test:GetMediaCaptureState", data => {
  sendAsyncMessage("Test:MediaCaptureState", _getMediaCaptureState());
});

addMessageListener("Test:WaitForObserverCall", ({data}) => {
  let topic = data;
  Services.obs.addObserver(function obs(aSubject, aTopic, aData) {
    if (aTopic != topic) {
      is(aTopic, topic, "Wrong topic observed");
      return;
    }

    if (ignoreEvent(aSubject, aTopic, aData)) {
      return;
    }

    sendAsyncMessage("Test:ObserverCalled", topic);
    Services.obs.removeObserver(obs, topic);

    if (kObservedTopics.indexOf(topic) != -1) {
      if (!(topic in gObservedTopics))
        gObservedTopics[topic] = -1;
      else
        --gObservedTopics[topic];
    }
  }, topic);
});

function messageListener({data}) {
  sendAsyncMessage("Test:MessageReceived", data);
}

addMessageListener("Test:WaitForMessage", () => {
  content.addEventListener("message", messageListener, {once: true});
});

addMessageListener("Test:WaitForMultipleMessages", () => {
  content.addEventListener("message", messageListener);
});

addMessageListener("Test:StopWaitForMultipleMessages", () => {
  content.removeEventListener("message", messageListener);
});
