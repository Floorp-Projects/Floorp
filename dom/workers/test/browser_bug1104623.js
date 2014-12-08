/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function whenBrowserLoaded(aBrowser, aCallback) {
  aBrowser.addEventListener("load", function onLoad(event) {
    if (event.target == aBrowser.contentDocument) {
      aBrowser.removeEventListener("load", onLoad, true);
      executeSoon(aCallback);
    }
  }, true);
}

function test() {
  waitForExplicitFinish();

  let testURL = "chrome://mochitests/content/chrome/dom/base/test/file_empty.html";

  let tab = gBrowser.addTab(testURL);
  gBrowser.selectedTab = tab;

  whenBrowserLoaded(tab.linkedBrowser, function() {
    let doc = tab.linkedBrowser.contentDocument;

    let blob = new tab.linkedBrowser.contentWindow.Blob(['onmessage = function() { postMessage(true); }']);
    ok(blob, "Blob has been created");

    let blobURL = tab.linkedBrowser.contentWindow.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new tab.linkedBrowser.contentWindow.Worker(blobURL);
    ok(worker, "Worker has been created");

    worker.onerror = function(error) {
      ok(false, "Worker.onerror:" + error.message);
      gBrowser.removeTab(tab);
      finish();
    }

    worker.onmessage = function() {
      ok(true, "Worker.onmessage");
      gBrowser.removeTab(tab);
      finish();
    }

    worker.postMessage(true);
  });
}
