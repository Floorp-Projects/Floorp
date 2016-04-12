/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/dom/broadcastchannel/tests/blank.html";

add_task(function*() {
  var win1 = OpenBrowserWindow({private: true});
  var win1Promise = new win1.Promise(resolve => {
    win1.addEventListener("load", function onLoad() {
      win1.removeEventListener("load", onLoad, false);
      resolve();
    });
  });
  yield win1Promise;

  var win2 = OpenBrowserWindow({private: false});
  var win2Promise = new win2.Promise(resolve => {
    win2.addEventListener("load", function onLoad() {
      win2.removeEventListener("load", onLoad, false);
      resolve();
    });
  });
  yield win2Promise;

  var tab1 = win1.gBrowser.addTab(URL);
  yield BrowserTestUtils.browserLoaded(win1.gBrowser.getBrowserForTab(tab1));
  var browser1 = gBrowser.getBrowserForTab(tab1);

  var tab2 = win2.gBrowser.addTab(URL);
  yield BrowserTestUtils.browserLoaded(win2.gBrowser.getBrowserForTab(tab2));
  var browser2 = gBrowser.getBrowserForTab(tab2);

  var p1 = ContentTask.spawn(browser1, null, function(opts) {
    return new content.window.Promise(resolve => {
      content.window.bc = new content.window.BroadcastChannel('foobar');
      content.window.bc.onmessage = function(e) { resolve(e.data); }
    });
  });

  var p2 = ContentTask.spawn(browser2, null, function(opts) {
    return new content.window.Promise(resolve => {
      content.window.bc = new content.window.BroadcastChannel('foobar');
      content.window.bc.onmessage = function(e) { resolve(e.data); }
    });
  });

  yield ContentTask.spawn(browser1, null, function(opts) {
    return new content.window.Promise(resolve => {
      var bc = new content.window.BroadcastChannel('foobar');
      bc.postMessage('hello world from private browsing');
      resolve();
    });
  });

  yield ContentTask.spawn(browser2, null, function(opts) {
    return new content.window.Promise(resolve => {
      var bc = new content.window.BroadcastChannel('foobar');
      bc.postMessage('hello world from non private browsing');
      resolve();
    });
  });

  var what1 = yield p1;
  ok(what1, 'hello world from private browsing', 'No messages received from the other window.');

  var what2 = yield p2;
  ok(what1, 'hello world from non private browsing', 'No messages received from the other window.');

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.closeWindow(win1);

  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.closeWindow(win2);
});
