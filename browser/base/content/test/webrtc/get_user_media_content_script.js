/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

const kObservedTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended",
];

var gObservedTopics = {};

function ignoreEvent(aSubject, aTopic, aData) {
  // With e10s disabled, our content script receives notifications for the
  // preview displayed in our screen sharing permission prompt; ignore them.
  const kBrowserURL = AppConstants.BROWSER_CHROME_URL;
  const nsIPropertyBag = Ci.nsIPropertyBag;
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
  let hasCamera = {};
  let hasMicrophone = {};
  let hasScreenShare = {};
  let hasWindowShare = {};
  let hasBrowserShare = {};
  MediaManagerService.mediaCaptureWindowState(content,
                                              hasCamera, hasMicrophone,
                                              hasScreenShare, hasWindowShare,
                                              hasBrowserShare);
  let result = {};

  if (hasCamera.value != MediaManagerService.STATE_NOCAPTURE)
    result.video = true;
  if (hasMicrophone.value != MediaManagerService.STATE_NOCAPTURE)
    result.audio = true;

  if (hasScreenShare.value != MediaManagerService.STATE_NOCAPTURE)
    result.screen = "Screen";
  else if (hasWindowShare.value != MediaManagerService.STATE_NOCAPTURE)
    result.screen = "Window";
  else if (hasBrowserShare.value != MediaManagerService.STATE_NOCAPTURE)
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

    if (kObservedTopics.includes(topic)) {
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
