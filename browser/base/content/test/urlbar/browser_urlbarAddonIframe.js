"use strict";

// The purpose of this test is to test the urlbar popup's add-on iframe.  It has
// a few parts:
//
// (1) This file, a normal browser mochitest.
// (2) html/js files that are loaded in the urlbar popup's add-on iframe:
//     urlbarAddonIframe.{html,js}
// (3) A content script that mediates between the first two parts:
//     urlbarAddonIframeContentScript.js
//
// The main test file (this file) sends messages to the content script, which
// forwards them as events to the iframe.  These messages tell the iframe js to
// do various things like call functions on the urlbar API and expect events.
// In response, the iframe js dispatches ack events to the content script, which
// forwards them as messages to the main test file.
//
// The content script may not be necessary right now since the iframe is not
// remote.  But this structure ensures that if the iframe is made remote in the
// future, then the test won't have to change very much, and ideally not at all.
//
// Actually there's one other part:
//
// (4) The Panel.jsm that's bundled with add-ons that use the iframe.
//
// Panel.jsm defines the API that's made available to add-on scripts running in
// the iframe.  This API is orthogonal to the add-on iframe itself.  You could
// load any html/js in the iframe, technically.  But the purpose of the iframe
// is to support this Panel.jsm API, so that's what this test tests.

const PANEL_JSM_BASENAME = "Panel.jsm";
const IFRAME_BASENAME = "urlbarAddonIframe.html";
const CONTENT_SCRIPT_BASENAME = "urlbarAddonIframeContentScript.js";

// The iframe's message manager.
let gMsgMan;

add_task(function* () {
  let rootDirURL = getRootDirectory(gTestPath);
  let jsmURL = rootDirURL + PANEL_JSM_BASENAME;
  let iframeURL = rootDirURL + IFRAME_BASENAME;
  let contentScriptURL = rootDirURL + CONTENT_SCRIPT_BASENAME;

  let { Panel } = Cu.import(jsmURL, {});
  let panel = new Panel(gURLBar.popup, iframeURL);
  registerCleanupFunction(() => {
    panel.destroy();
    Assert.ok(gURLBar.popup._addonIframe === null, "iframe should be gone");
  });

  let iframe = gURLBar.popup._addonIframe;
  Assert.ok(!!iframe, "iframe should not be null");

  gMsgMan =
    iframe.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
  gMsgMan.loadFrameScript(contentScriptURL, false);

  yield promiseIframeLoad();

  // urlbar.getValue
  let value = "this value set by the test";
  gURLBar.value = value;
  let readValue = yield promiseUrlbarFunctionCall("getValue");
  Assert.equal(readValue, value, "value");

  // urlbar.setValue
  value = "this value set by the iframe";
  yield promiseUrlbarFunctionCall("setValue", value);
  Assert.equal(gURLBar.value, value, "setValue");

  // urlbar.getMaxResults
  let maxResults = gURLBar.popup.maxResults;
  Assert.equal(typeof(maxResults), "number", "Sanity check");
  let readMaxResults = yield promiseUrlbarFunctionCall("getMaxResults");
  Assert.equal(readMaxResults, maxResults, "getMaxResults");

  // urlbar.setMaxResults
  let newMaxResults = maxResults + 10;
  yield promiseUrlbarFunctionCall("setMaxResults", newMaxResults);
  Assert.equal(gURLBar.popup.maxResults, newMaxResults, "setMaxResults");
  gURLBar.popup.maxResults = maxResults;

  // urlbar.enter
  value = "http://mochi.test:8888/";
  yield promiseUrlbarFunctionCall("setValue", value);
  Assert.equal(gURLBar.value, value, "setValue");
  yield promiseUrlbarFunctionCall("enter");
  let browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);
  Assert.equal(browser.currentURI.spec, value,
               "enter should have loaded the URL");

  // input, reset, and result events.  There should always be at least one
  // result, the heuristic result.
  value = "test";
  let promiseValues = yield Promise.all([
    promiseEvent("input")[1],
    promiseEvent("reset")[1],
    promiseEvent("result")[1],
    promiseAutocompleteResultPopup(value, window, true),
  ]);

  // Check the heuristic result.
  let result = promiseValues[2];
  let engineName = Services.search.currentEngine.name;
  Assert.equal(result.url,
               `moz-action:searchengine,{"engineName":"${engineName}","input":"test","searchQuery":"test"}`,
               "result.url");
  Assert.ok("action" in result, "result.action");
  Assert.equal(result.action.type, "searchengine", "result.action.type");
  Assert.ok("params" in result.action, "result.action.params");
  Assert.equal(result.action.params.engineName, engineName,
               "result.action.params.engineName");
  Assert.equal(typeof(result.image), "string", "result.image");
  Assert.equal(result.title, engineName, "result.title");
  Assert.equal(result.type, "action searchengine heuristic", "result.type");
  Assert.equal(result.text, value, "result.text");

  // keydown event.  promiseEvent sends an async message to the iframe, but
  // synthesizeKey is sync, so we need to wait until the content JS receives
  // the message and adds its event listener before synthesizing the key.
  let keydownPromises = promiseEvent("keydown");
  yield keydownPromises[0];
  EventUtils.synthesizeKey("KEY_ArrowDown", {
    type: "keydown",
    code: "ArrowDown",
  });
  yield keydownPromises[1];

  // urlbar.getPanelHeight
  let height = iframe.getBoundingClientRect().height;
  let readHeight = yield promiseUrlbarFunctionCall("getPanelHeight");
  Assert.equal(readHeight, height, "getPanelHeight");

  // urlbar.setPanelHeight
  let newHeight = height + 100;
  yield promiseUrlbarFunctionCall("setPanelHeight", newHeight);
  yield new Promise(resolve => {
    // The height change is animated, so give it time to complete.  Again, wait
    // a sec to be safe.
    setTimeout(resolve, 1000);
  });
  Assert.equal(iframe.getBoundingClientRect().height, newHeight,
               "setPanelHeight");
});

function promiseIframeLoad() {
  let msgName = "TestIframeLoadAck";
  return new Promise(resolve => {
    info("Waiting for iframe load ack");
    gMsgMan.addMessageListener(msgName, function onMsg(msg) {
      info("Received iframe load ack");
      gMsgMan.removeMessageListener(msgName, onMsg);
      resolve();
    });
  });
}

/**
 * Returns a single promise that's resolved when the content JS has called the
 * function.
 */
function promiseUrlbarFunctionCall(...args) {
  return promiseMessage("function", args)[0];
}

/**
 * Returns two promises in an array.  The first is resolved when the content JS
 * has added its event listener.  The second is resolved when the content JS
 * has received the event.
 */
function promiseEvent(type) {
  return promiseMessage("event", type, 2);
}

let gNextMessageID = 1;

/**
 * Returns an array of promises, one per ack.  Each is resolved when the content
 * JS acks the message.  numExpectedAcks is the number of acks you expect.
 */
function promiseMessage(type, data, numExpectedAcks = 1) {
  let testMsgName = "TestMessage";
  let ackMsgName = "TestMessageAck";
  let msgID = gNextMessageID++;
  gMsgMan.sendAsyncMessage(testMsgName, {
    type,
    messageID: msgID,
    data,
  });
  let ackPromises = [];
  for (let i = 0; i < numExpectedAcks; i++) {
    let ackIndex = i;
    ackPromises.push(new Promise(resolve => {
      info("Waiting for message ack: " + JSON.stringify({
        type,
        msgID,
        ackIndex,
      }));
      gMsgMan.addMessageListener(ackMsgName, function onMsg(msg) {
        // Messages have IDs so that an ack can be correctly paired with the
        // initial message it's replying to.  It's not an error if the ack's ID
        // isn't equal to msgID here.  That will happen when multiple messages
        // have been sent in a single turn of the event loop so that they're all
        // waiting on acks.  Same goes for ackIndex.
        if (msg.data.messageID != msgID || msg.data.ackIndex != ackIndex) {
          return;
        }
        info("Received message ack: " + JSON.stringify({
          type,
          msgID: msg.data.messageID,
          ackIndex,
        }));
        gMsgMan.removeMessageListener(ackMsgName, onMsg);
        resolve(msg.data.data);
      });
    }));
  }
  return ackPromises;
}
