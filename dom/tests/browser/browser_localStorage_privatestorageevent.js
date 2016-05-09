add_task(function *() {
  var privWin = OpenBrowserWindow({private: true});
  yield new privWin.Promise(resolve => {
    privWin.addEventListener('load', function onLoad() {
      privWin.removeEventListener('load', onLoad, false);
      resolve();
    });
  });

  var pubWin = OpenBrowserWindow({private: false});
  yield new pubWin.Promise(resolve => {
    pubWin.addEventListener('load', function onLoad() {
      pubWin.removeEventListener('load', onLoad, false);
      resolve();
    });
  });

  var URL = "http://mochi.test:8888/browser/dom/tests/browser/page_privatestorageevent.html";

  var privTab = privWin.gBrowser.addTab(URL);
  yield BrowserTestUtils.browserLoaded(privWin.gBrowser.getBrowserForTab(privTab));
  var privBrowser = gBrowser.getBrowserForTab(privTab);

  var pubTab = pubWin.gBrowser.addTab(URL);
  yield BrowserTestUtils.browserLoaded(pubWin.gBrowser.getBrowserForTab(pubTab));
  var pubBrowser = gBrowser.getBrowserForTab(pubTab);

  // Check if pubWin can see privWin's storage events
  yield ContentTask.spawn(pubBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  yield ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.localStorage['key'] = 'ablooabloo';
  });

  let pubSaw = yield ContentTask.spawn(pubBrowser, null, function(opts) {
    return content.window.gotStorageEvent;
  });

  ok(!pubSaw, "pubWin shouldn't be able to see privWin's storage events");

  yield ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  // Check if privWin can see pubWin's storage events
  yield ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  yield ContentTask.spawn(pubBrowser, null, function(opts) {
    content.window.localStorage['key'] = 'ablooabloo';
  });

  let privSaw = yield ContentTask.spawn(privBrowser, null, function(opts) {
    return content.window.gotStorageEvent;
  });

  ok(!privSaw, "privWin shouldn't be able to see pubWin's storage events");

  yield BrowserTestUtils.removeTab(privTab);
  yield BrowserTestUtils.closeWindow(privWin);

  yield BrowserTestUtils.removeTab(pubTab);
  yield BrowserTestUtils.closeWindow(pubWin);
});
