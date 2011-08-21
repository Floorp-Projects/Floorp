function test() {
  waitForExplicitFinish();
  testNext();
}

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
  ["ftp.example.bar", "ftp://ftp.example.bar/"],
  ["ex ample", Services.search.originalDefaultEngine.getSubmission("ex ample").uri.spec],
];

function testNext() {
  if (!pairs.length) {
    finish();
    return;
  }

  let [inputValue, expectedURL] = pairs.shift();

  gBrowser.addProgressListener({
    onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
        is(aRequest.originalURI.spec, expectedURL,
           "entering '" + inputValue + "' loads expected URL");

        gBrowser.removeProgressListener(this);
        gBrowser.stop();

        executeSoon(testNext);
      }
    }
  });

  gURLBar.addEventListener("focus", function onFocus() {
    gURLBar.removeEventListener("focus", onFocus);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });
  });

  gBrowser.selectedBrowser.focus();
  gURLBar.inputField.value = inputValue;
  gURLBar.focus();
}
