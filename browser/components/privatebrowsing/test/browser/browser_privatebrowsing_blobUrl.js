"use strict";

// Here we want to test that blob URLs are not available between private and
// non-private browsing.

const BASE_URI =
  "http://mochi.test:8888/browser/browser/components/" +
  "privatebrowsing/test/browser/empty_file.html";

add_task(async function test() {
  const loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    BASE_URI
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, BASE_URI);
  await loaded;

  let blobURL;
  info("Creating a blob URL...");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return Promise.resolve(
      content.window.URL.createObjectURL(
        new Blob([123], { type: "text/plain" })
      )
    );
  }).then(newURL => {
    blobURL = newURL;
  });

  info("Blob URL: " + blobURL);

  info("Creating a private window...");

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let privateTab = privateWin.gBrowser.selectedBrowser;

  const privateTabLoaded = BrowserTestUtils.browserLoaded(
    privateTab,
    false,
    BASE_URI
  );
  BrowserTestUtils.startLoadingURIString(privateTab, BASE_URI);
  await privateTabLoaded;

  await SpecialPowers.spawn(privateTab, [blobURL], function (url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.onerror = function () {
        resolve("SendErrored");
      };
      xhr.onload = function () {
        resolve("SendLoaded");
      };
      xhr.open("GET", url);
      xhr.send();
    });
  }).then(status => {
    is(
      status,
      "SendErrored",
      "Using a blob URI from one user context id in another should not work"
    );
  });

  await BrowserTestUtils.closeWindow(privateWin);
});
