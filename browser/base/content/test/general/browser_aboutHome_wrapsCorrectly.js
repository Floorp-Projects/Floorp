function waitForContentEvent(browser, eventName, capture, checkFn) {
  let parameters = { eventName,
                     capture,
                     checkFnSource: checkFn ? checkFn.toSource() : null };
  return ContentTask.spawn(browser, parameters,
      function({ eventName, capture, checkFnSource }) {
        let checkFn;
        if (checkFnSource) {
          checkFn = eval(`(() => (${checkFnSource}))()`);
        }
        return new Promise((resolve, reject) => {
          addEventListener(eventName, function listener(event) {
            let completion = resolve;
            try {
              if (checkFn && !checkFn(event)) {
                return;
              }
            } catch (e) {
              completion = () => reject(e);
            }
            removeEventListener(eventName, listener, capture);
            completion();
          }, capture);
        });
      });
}

add_task(function* () {
  let newWindow = yield BrowserTestUtils.openNewBrowserWindow();

  let resizedPromise = BrowserTestUtils.waitForEvent(newWindow, "resize");
  newWindow.resizeTo(300, 300);
  yield resizedPromise;

  yield BrowserTestUtils.openNewForegroundTab(newWindow.gBrowser, "about:home");

  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    is(content.document.body.getAttribute("narrow"), "true", "narrow mode");
  });

  resizedPromise = waitForContentEvent(newWindow.gBrowser.selectedBrowser, "resize");

  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    content.window.resizeTo(800, 800);
  });

  yield resizedPromise;

  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    is(content.document.body.hasAttribute("narrow"), false, "non-narrow mode");
  });

  yield BrowserTestUtils.closeWindow(newWindow);
});
