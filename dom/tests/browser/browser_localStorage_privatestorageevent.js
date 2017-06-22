add_task(async function() {
  var privWin = OpenBrowserWindow({private: true});
  await new privWin.Promise(resolve => {
    privWin.addEventListener('load', function() {
      resolve();
    }, {once: true});
  });

  var pubWin = OpenBrowserWindow({private: false});
  await new pubWin.Promise(resolve => {
    pubWin.addEventListener('load', function() {
      resolve();
    }, {once: true});
  });

  var URL = "http://mochi.test:8888/browser/dom/tests/browser/page_privatestorageevent.html";

  var privTab = privWin.gBrowser.addTab(URL);
  await BrowserTestUtils.browserLoaded(privWin.gBrowser.getBrowserForTab(privTab));
  var privBrowser = gBrowser.getBrowserForTab(privTab);

  var pubTab = pubWin.gBrowser.addTab(URL);
  await BrowserTestUtils.browserLoaded(pubWin.gBrowser.getBrowserForTab(pubTab));
  var pubBrowser = gBrowser.getBrowserForTab(pubTab);

  // Check if pubWin can see privWin's storage events
  await ContentTask.spawn(pubBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  await ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.localStorage['key'] = 'ablooabloo';
  });

  let pubSaw = await ContentTask.spawn(pubBrowser, null, function(opts) {
    return content.window.gotStorageEvent;
  });

  ok(!pubSaw, "pubWin shouldn't be able to see privWin's storage events");

  await ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  // Check if privWin can see pubWin's storage events
  await ContentTask.spawn(privBrowser, null, function(opts) {
    content.window.gotStorageEvent = false;
    content.window.addEventListener('storage', ev => {
      content.window.gotStorageEvent = true;
    });
  });

  await ContentTask.spawn(pubBrowser, null, function(opts) {
    content.window.localStorage['key'] = 'ablooabloo';
  });

  let privSaw = await ContentTask.spawn(privBrowser, null, function(opts) {
    return content.window.gotStorageEvent;
  });

  ok(!privSaw, "privWin shouldn't be able to see pubWin's storage events");

  await BrowserTestUtils.removeTab(privTab);
  await BrowserTestUtils.closeWindow(privWin);

  await BrowserTestUtils.removeTab(pubTab);
  await BrowserTestUtils.closeWindow(pubWin);
});
