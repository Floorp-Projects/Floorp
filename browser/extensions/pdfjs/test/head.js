function waitForPdfJS(browser) {
  // Runs tests after all 'load' event handlers have fired off
  return ContentTask.spawn(browser, null, function* () {
    yield new Promise((resolve) => {
      content.addEventListener("documentload", function listener() {
        content.removeEventListener("documentload", listener, false);
        resolve();
      }, false, true);
    });
  });
}
