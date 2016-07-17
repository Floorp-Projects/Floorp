const FRAME_URL = "http://example.org/";

const METHODS = {
  setVisible: {},
  getVisible: {},
  setActive: {},
  getActive: {},
  addNextPaintListener: {},
  removeNextPaintListener: {},
  sendMouseEvent: {},
  sendTouchEvent: {},
  goBack: {},
  goForward: {},
  reload: {},
  stop: {},
  download: {},
  purgeHistory: {},
  getScreenshot: {},
  zoom: {},
  getCanGoBack: {},
  getCanGoForward: {},
  getContentDimensions: {},
  setInputMethodActive: {},
  setNFCFocus: {},
  findAll: {},
  findNext: {},
  clearMatch: {},
  executeScript: {},
  getWebManifest: {},
  mute: {},
  unmute: {},
  getMuted: {},
  setVolume: {},
  getVolume: {},
};

const ATTRIBUTES = [
  "allowedAudioChannels",
];

function once(target, eventName, useCapture = false) {
  info("Waiting for event: '" + JSON.stringify(eventName) + "' on " + target + ".");

  return new Promise(resolve => {
    for (let [add, remove] of [
      ["addEventListener", "removeEventListener"],
      ["addMessageListener", "removeMessageListener"],
    ]) {
      if ((add in target) && (remove in target)) {
        eventName.forEach(evName => {
          target[add](evName, function onEvent(...aArgs) {
            info("Got event: '" + evName + "' on " + target + ".");
            target[remove](evName, onEvent, useCapture);
            resolve(aArgs);
          }, useCapture);
	});
        break;
      }
    }
  });
}

function* loadFrame(attributes = {}) {
  let iframe = document.createElement("iframe");
  iframe.setAttribute("src", FRAME_URL);
  for (let key in attributes) {
    iframe.setAttribute(key, attributes[key]);
  }
  let loaded = once(iframe, [ "load", "mozbrowserloadend" ]);
  document.body.appendChild(iframe);
  yield loaded;
  return iframe;
}
