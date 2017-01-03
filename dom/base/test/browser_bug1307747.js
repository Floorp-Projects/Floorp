/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

var {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

const BASE_URI = "http://mochi.test:8888/browser/dom/base/test/empty.html";

add_task(function* test_initialize() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser = gBrowser.getBrowserForTab(tab);

  let blob = yield ContentTask.spawn(browser, null, function() {
    let blob = new Blob([new Array(1024*1024).join('123456789ABCDEF')]);
    return blob;
  });

  ok(blob, "We have a blob!");
  is(blob.size, new Array(1024*1024).join('123456789ABCDEF').length, "The size matches");

  let status = yield ContentTask.spawn(browser, blob, function(blob) {
    return new Promise(resolve => {
      let fr = new content.FileReader();
      fr.readAsText(blob);
      fr.onloadend = function() {
        resolve(fr.result == new Array(1024*1024).join('123456789ABCDEF'));
      }
    });
  });

  ok(status, "Data match!");

  yield BrowserTestUtils.removeTab(tab);
});
