"use strict";

// Here we want to test that blob URLs are not available between private and
// non-private browsing.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "privatebrowsing/test/browser/empty_file.html";

add_task(function* test() {
  info("Creating a normal window...");
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let tab = win.gBrowser.selectedBrowser;
  tab.loadURI(BASE_URI);
  yield BrowserTestUtils.browserLoaded(tab);

  let blobURL;

  info("Creating a blob URL...");
  yield ContentTask.spawn(tab, null, function() {
    return Promise.resolve(content.window.URL.createObjectURL(new content.window.Blob([123])));
  }).then(newURL => { blobURL = newURL });

  info("Blob URL: " + blobURL);

  info("Creating a private window...");
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  let privateTab = privateWin.gBrowser.selectedBrowser;
  privateTab.loadURI(BASE_URI);
  yield BrowserTestUtils.browserLoaded(privateTab);

  yield ContentTask.spawn(privateTab, blobURL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      try {
        xhr.open("GET", url);
        resolve("OpenSucceeded");
      } catch(e) {
        resolve("OpenThrew");
      }
    });
  }).then(status => {
    is(status, "OpenThrew", "Using a blob URI from one user context id in another should not work");
  });

  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(privateWin);
});
