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
  // Disable autoFill for this test, since it could mess up the results.
  let autoFill = Preferences.get("browser.urlbar.autoFill");
  Preferences.set("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => {
    Preferences.set("browser.urlbar.autoFill", autoFill);
  });

  for (let [inputValue, expectedURL] of pairs) {
    let promiseLoad = waitForDocLoadAndStopIt(expectedURL);
    gURLBar.focus();
    gURLBar.inputField.value = inputValue.slice(0, -1);
    EventUtils.synthesizeKey(inputValue.slice(-1), {});
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });
    yield promiseLoad;
  }
});
