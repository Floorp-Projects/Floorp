function waitForPdfJS(browser, url) {
  // Runs tests after all 'load' event handlers have fired off
  return ContentTask.spawn(browser, url, function* (url) {
    yield new Promise((resolve) => {
      // NB: Add the listener to the global object so that we receive the
      // event fired from the new window.
      addEventListener("documentload", function listener() {
        removeEventListener("documentload", listener, false);
        resolve();
      }, false, true);

      content.location = url;
    });
  });
}
