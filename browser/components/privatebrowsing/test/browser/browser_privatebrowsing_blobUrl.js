"use strict";

// Here we want to test that blob URLs are not available between private and
// non-private browsing.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "privatebrowsing/test/browser/empty_file.html";

add_task(async function test() {
  info("Creating a normal window...");
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab = win.gBrowser.selectedBrowser;
  tab.loadURI(BASE_URI);
  await BrowserTestUtils.browserLoaded(tab);

  let blobURL;

  info("Creating a blob URL...");
  await ContentTask.spawn(tab, null, function() {
    return Promise.resolve(content.window.URL.createObjectURL(new content.window.Blob([123])));
  }).then(newURL => { blobURL = newURL; });

  info("Blob URL: " + blobURL);

  info("Creating a private window...");
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let privateTab = privateWin.gBrowser.selectedBrowser;
  privateTab.loadURI(BASE_URI);
  await BrowserTestUtils.browserLoaded(privateTab);

  await ContentTask.spawn(privateTab, blobURL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.onerror = function() { resolve("SendErrored"); };
      xhr.onload = function() { resolve("SendLoaded"); };
      xhr.open("GET", url);
      xhr.send();
    });
  }).then(status => {
    is(status, "SendErrored", "Using a blob URI from one user context id in another should not work");
  });

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(privateWin);
});
