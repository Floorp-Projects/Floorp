add_task(function*() {
  // Make the min_background_timeout_value very high to avoid problems on slow machines
  yield new Promise(resolve => SpecialPowers.pushPrefEnv({
    'set': [['dom.min_background_timeout_value', 3000]]
  }, resolve));

  // Make a new tab, and put it in the background
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    yield BrowserTestUtils.withNewTab("about:blank", function*() {
      let time = yield ContentTask.spawn(browser, null, function () {
        return new Promise(resolve => {
          let start = content.performance.now();
          let id = content.window.setInterval(function() {
            let end = content.performance.now();
            content.window.clearInterval(id);
            resolve(end - start);
          }, 0);
        });
      });

      ok(time > 2000, "Interval is throttled with no webaudio (" + time + " ms)");

      time = yield ContentTask.spawn(browser, null, function () {
        return new Promise(resolve => {
          // Create an audio context, and save it on the window so it doesn't get GCed
          content.window._audioCtx = new content.window.AudioContext();

          let start = content.performance.now();
          let id = content.window.setInterval(function() {
            let end = content.performance.now();
            content.window.clearInterval(id);
            resolve(end - start);
          }, 0);
        });
      });

      ok(time < 1000, "Interval is not throttled with an audio context present (" + time + " ms)");
    });
  });
});
