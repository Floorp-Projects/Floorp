/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/dom/broadcastchannel/tests/blank.html";

add_task(async function() {
  var win1 = OpenBrowserWindow({private: true});
  var win1Promise = new win1.Promise(resolve => {
    win1.addEventListener("load", function() {
      resolve();
    }, {once: true});
  });
  await win1Promise;

  var win2 = OpenBrowserWindow({private: false});
  var win2Promise = new win2.Promise(resolve => {
    win2.addEventListener("load", function() {
      resolve();
    }, {once: true});
  });
  await win2Promise;

  var tab1 = win1.gBrowser.addTab(URL);
  await BrowserTestUtils.browserLoaded(win1.gBrowser.getBrowserForTab(tab1));
  var browser1 = gBrowser.getBrowserForTab(tab1);

  var tab2 = win2.gBrowser.addTab(URL);
  await BrowserTestUtils.browserLoaded(win2.gBrowser.getBrowserForTab(tab2));
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

  await ContentTask.spawn(browser1, null, function(opts) {
    return new content.window.Promise(resolve => {
      var bc = new content.window.BroadcastChannel('foobar');
      bc.postMessage('hello world from private browsing');
      resolve();
    });
  });

  await ContentTask.spawn(browser2, null, function(opts) {
    return new content.window.Promise(resolve => {
      var bc = new content.window.BroadcastChannel('foobar');
      bc.postMessage('hello world from non private browsing');
      resolve();
    });
  });

  var what1 = await p1;
  ok(what1, 'hello world from private browsing', 'No messages received from the other window.');

  var what2 = await p2;
  ok(what1, 'hello world from non private browsing', 'No messages received from the other window.');

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.closeWindow(win1);

  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.closeWindow(win2);
});
