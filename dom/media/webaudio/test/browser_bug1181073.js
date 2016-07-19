add_task(function*() {
  // Make the min_background_timeout_value very high to avoid problems on slow machines
  yield new Promise(resolve => SpecialPowers.pushPrefEnv({
    'set': [['dom.min_background_timeout_value', 3000]]
  }, resolve));

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");
  let browser = gBrowser.selectedBrowser;

  // Make the tab a background tab, so that setInterval will be throttled.
  yield BrowserTestUtils.openNewForegroundTab(gBrowser);

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

  // Set up a listener for the oscillator's demise
  let oscillatorDemisePromise = ContentTask.spawn(browser, null, function() {
    return new Promise(resolve => {
      let observer = () => resolve();
      // Record the observer on the content object so we can throw it out later
      content.__bug1181073_observer = observer;
      Services.obs.addObserver(observer, "webaudio-node-demise", false);
    });
  });

  time = yield ContentTask.spawn(browser, null, function () {
    return new Promise(resolve => {
      // Start playing audio, save it on the window so it doesn't get GCed
      let audioCtx = content.window.audioCtx = new content.window.AudioContext();
      let oscillator = audioCtx.createOscillator();
      oscillator.type = 'square';
      oscillator.frequency.value = 3000;
      oscillator.start();

      let start = content.performance.now();
      let id = content.window.setInterval(function() {
        let end = content.performance.now();
        content.window.clearInterval(id);
        oscillator.stop();
        resolve(end - start);
      }, 0);
    });
  });

  ok(time < 1000, "Interval is not throttled with audio playing (" + time + " ms)");

  // Destroy the oscillator, but not the audio context
  yield new Promise(resolve => SpecialPowers.exactGC(resolve));
  yield oscillatorDemisePromise;

  time = yield ContentTask.spawn(browser, null, function () {
    return new Promise(resolve => {
      let start = content.performance.now();
      let id = content.window.setInterval(function() {
        let end = content.performance.now();
        content.window.clearInterval(id);
        resolve(end - start);
      }, 0);
    });
  });

  ok(time > 2000, "Interval is throttled with audio stopped (" + time + " ms)");

  while (gBrowser.tabs.length > 1)
    gBrowser.removeCurrentTab();
});
