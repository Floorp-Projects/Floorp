add_task(function*() {
  let testcases = [
    ["example", "http://www.example.net/", { shiftKey: true }],
    // Check that a direct load is not overwritten by a previous canonization.
    ["http://example.com/test/", "http://example.com/test/", {}],
    ["ex-ample", "http://www.ex-ample.net/", { shiftKey: true }],
    ["  example ", "http://www.example.net/", { shiftKey: true }],
    [" example/foo ", "http://www.example.net/foo", { shiftKey: true }],
    [" example/foo bar ", "http://www.example.net/foo%20bar", { shiftKey: true }],
    ["example.net", "http://example.net/", { shiftKey: true }],
    ["http://example", "http://example/", { shiftKey: true }],
    ["example:8080", "http://example:8080/", { shiftKey: true }],
    ["ex-ample.foo", "http://ex-ample.foo/", { shiftKey: true }],
    ["example.foo/bar ", "http://example.foo/bar", { shiftKey: true }],
    ["1.1.1.1", "http://1.1.1.1/", { shiftKey: true }],
    ["ftp://example", "ftp://example/", { shiftKey: true }],
    ["ftp.example.bar", "http://ftp.example.bar/", { shiftKey: true }],
    ["ex ample", Services.search.defaultEngine.getSubmission("ex ample", null, "keyword").uri.spec, { shiftKey: true }],
  ];

  // Disable autoFill for this test, since it could mess up the results.
  let autoFill = Preferences.get("browser.urlbar.autoFill");
  Preferences.set("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => {
    Preferences.set("browser.urlbar.autoFill", autoFill);
  });

  for (let [inputValue, expectedURL, options] of testcases) {
    let promiseLoad = waitForDocLoadAndStopIt(expectedURL);
    gURLBar.focus();
    if (Object.keys(options).length > 0) {
      gURLBar.selectionStart = gURLBar.selectionEnd =
        gURLBar.inputField.value.length;
      gURLBar.inputField.value = inputValue.slice(0, -1);
      EventUtils.synthesizeKey(inputValue.slice(-1), {});
    } else {
      gURLBar.textValue = inputValue;
    }
    EventUtils.synthesizeKey("VK_RETURN", options);
    yield promiseLoad;
  }
});
