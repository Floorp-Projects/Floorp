var pairs = [
  ["example", "http://www.example.net/"],
  ["ex-ample", "http://www.ex-ample.net/"],
  ["  example ", "http://www.example.net/"],
  [" example/foo ", "http://www.example.net/foo"],
  [" example/foo bar ", "http://www.example.net/foo%20bar"],
  ["example.net", "http://example.net/"],
  ["http://example", "http://example/"],
  ["example:8080", "http://example:8080/"],
  ["ex-ample.foo", "http://ex-ample.foo/"],
  ["example.foo/bar ", "http://example.foo/bar"],
  ["1.1.1.1", "http://1.1.1.1/"],
  ["ftp://example", "ftp://example/"],
  ["ftp.example.bar", "http://ftp.example.bar/"],
  ["ex ample", Services.search.defaultEngine.getSubmission("ex ample", null, "keyword").uri.spec],
];

add_task(function*() {
  for (let [inputValue, expectedURL] of pairs) {
    let focusEventPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
    let messagePromise = BrowserTestUtils.waitForMessage(gBrowser.selectedBrowser.messageManager,
                                                         "browser_canonizeURL:start");

    let stoppedLoadPromise = ContentTask.spawn(gBrowser.selectedBrowser, [inputValue, expectedURL],
      function([inputValue, expectedURL]) {
        return new Promise(resolve => {
          let wpl = {
            onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
              if (aStateFlags & Ci.nsIWebProgressListener.STATE_START &&
                  aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
                if (!aRequest || !(aRequest instanceof Ci.nsIChannel)) {
                  return;
                }
                aRequest.QueryInterface(Ci.nsIChannel);
                is(aRequest.originalURI.spec, expectedURL,
                   "entering '" + inputValue + "' loads expected URL");

                webProgress.removeProgressListener(filter);
                filter.removeProgressListener(wpl);
                docShell.QueryInterface(Ci.nsIWebNavigation);
                docShell.stop(docShell.STOP_ALL);
                resolve();
              }
            },
          };
          let filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                           .createInstance(Ci.nsIWebProgress);
          filter.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);

          let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIWebProgress);
          webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
          // We're sending this off to trigger the start of the this test, when all the
          // listeners are in place:
          sendAsyncMessage("browser_canonizeURL:start");
        });
      }
    );

    gBrowser.selectedBrowser.focus();
    gURLBar.focus();

    yield Promise.all([focusEventPromise, messagePromise]);

    gURLBar.inputField.value = inputValue.slice(0, -1);
    EventUtils.synthesizeKey(inputValue.slice(-1), {});
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });
    yield stoppedLoadPromise;
  }
});
