const FRAME_URL = "http://example.org/";

const METHODS = {
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
  findAll: {},
  findNext: {},
  clearMatch: {},
  executeScript: {},
  getWebManifest: {},
};

const ATTRIBUTES = [];

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

async function loadFrame(attributes = {}) {
  let iframe = document.createElement("iframe");
  iframe.setAttribute("src", FRAME_URL);
  for (let key in attributes) {
    iframe.setAttribute(key, attributes[key]);
  }
  let loaded = once(iframe, [ "load", "mozbrowserloadend" ]);
  document.body.appendChild(iframe);
  await loaded;
  return iframe;
}
