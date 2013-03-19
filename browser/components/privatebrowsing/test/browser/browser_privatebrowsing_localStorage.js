/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  function checkLocalStorage(aWindow, aCallback) {
    executeSoon(function() {
      let tab = aWindow.gBrowser.selectedTab = aWindow.gBrowser.addTab();
      let browser = aWindow.gBrowser.selectedBrowser;
      browser.addEventListener('load', function() {
        browser.removeEventListener('load', arguments.callee, true);
        let tab2 = aWindow.gBrowser.selectedTab = aWindow.gBrowser.addTab();
        browser.contentWindow.location = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/' +
                         'browser_privatebrowsing_localStorage_page2.html';
        browser.addEventListener('load', function() {
          browser.removeEventListener('load', arguments.callee, true);
          is(browser.contentWindow.document.title, '2', "localStorage should contain 2 items");
          aCallback();
        }, true);
      }, true);

      browser.loadURI('http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/' +
                      'browser_privatebrowsing_localStorage_page1.html');
    });
  }

  let windowsToClose = [];
  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      callback(win);
    }, false);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow({private: true}, function(win) {
    checkLocalStorage(win, finish);
  });

}
