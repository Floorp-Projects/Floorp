const url = "https://example.com/browser/dom/base/test/file_docgroup_forbid.html";

function frameScript() {
  let e = Services.ww.getWindowEnumerator();
  let exception = false;
  while (e.hasMoreElements()) {
    try {
      /*
       * If this is a window we're not supposed to touch, we'll get an
       * error very early here (during the QI).
       */
      var window = e.getNext().QueryInterface(Components.interfaces.nsIDOMWindow);
      var doc = window.document;
    } catch (e) {
      if (/accessing object in wrong DocGroup/.test(e.toString())) {
        exception = true;
        break;
      }
      throw e;
    }

    /*
     * Do some stuff that will trigger the DocGroup assertions if we
     * didn't throw.
     */

    let elt = doc.createElement("div");
    elt.innerHTML = "hello!";
    doc.body.appendChild(elt);

    let evt = new window.CustomEvent("foopy");
    doc.dispatchEvent(evt);
  }
  sendAsyncMessage("DocGroupTest:Done", exception);
}

function promiseMessage(messageManager, message) {
  return new Promise(resolve => {
    let listener = (msg) => {
      messageManager.removeMessageListener(message, listener);
      resolve(msg);
    };

    messageManager.addMessageListener(message, listener);
  })
}

add_task(function*() {
  // This pref is normally disabled during tests, but we want to test
  // it here, so we enable it.
  yield new Promise(go => {
    SpecialPowers.pushPrefEnv({set: [["extensions.throw_on_docgroup_mismatch.enabled", true]]}, go)
  });

  let url1 = url + "?tab=1";
  let url2 = url + "?tab=2";

  let browser1 = gBrowser.selectedBrowser;

  let tab2 = gBrowser.addTab(url2, {sameProcessAsFrameLoader: browser1.frameLoader});
  let browser2 = tab2.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser2, false, url2);

  browser1.loadURI(url1);
  yield BrowserTestUtils.browserLoaded(browser1, false, url1);

  browser1.messageManager.loadFrameScript(`data:,(${frameScript})();`, false);

  let exception = yield promiseMessage(browser1.messageManager, "DocGroupTest:Done");

  ok(exception, "Touching two windows threw an exception (that's good!)");

  gBrowser.removeTab(tab2);
});
