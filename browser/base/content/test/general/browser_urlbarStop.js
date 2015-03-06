"use strict";

const goodURL = "http://mochi.test:8888/";
const badURL = "http://mochi.test:8888/whatever.html";

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab(goodURL);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page");

  yield typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page after stop()");
  gBrowser.removeCurrentTab();

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  is(gURLBar.textValue, "", "location bar is empty");

  yield typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(badURL), "location bar reflects stopped page in an empty tab");
  gBrowser.removeCurrentTab();
});

function typeAndSubmitAndStop(url) {
  gBrowser.userTypedValue = url;
  URLBarSetURI();
  is(gURLBar.textValue, gURLBar.trimValue(url), "location bar reflects loading page");

  let promise = waitForDocLoadAndStopIt();
  gURLBar.handleCommand();
  return promise;
}

function waitForDocLoadAndStopIt() {
  function content_script() {
    const {interfaces: Ci, utils: Cu} = Components;
    Cu.import("resource://gre/modules/XPCOMUtils.jsm");

    let progressListener = {
      onStateChange(webProgress, req, flags, status) {
        if (flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          /* Hammer time. */
          content.stop();

          /* Let the parent know we're done. */
          sendAsyncMessage("{MSG}");
        }
      },

      QueryInterface: XPCOMUtils.generateQI(["nsISupportsWeakReference"])
    };

    let wp = docShell.QueryInterface(Ci.nsIWebProgress);
    wp.addProgressListener(progressListener, wp.NOTIFY_ALL);
  }

  return new Promise(resolve => {
    const MSG = "test:waitForDocLoadAndStopIt";
    const SCRIPT = content_script.toString().replace("{MSG}", MSG);

    let mm = gBrowser.selectedBrowser.messageManager;
    mm.loadFrameScript("data:,(" + SCRIPT + ")();", true);
    mm.addMessageListener(MSG, function onComplete() {
      mm.removeMessageListener(MSG, onComplete);
      resolve();
    });
  });
}
