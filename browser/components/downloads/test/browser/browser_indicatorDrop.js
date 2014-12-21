/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, "HttpServer",
  "resource://testing-common/httpd.js");

registerCleanupFunction(function*() {
  yield task_resetState();
  yield task_clearHistory();
});

add_task(function* test_indicatorDrop() {
  let downloadButton = document.getElementById("downloads-button");
  ok(downloadButton, "download button present");

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
      getService(Ci.mozIJSSubScriptLoader);
  let EventUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  function* task_drop(urls) {
    let dragData = [[{type: "text/plain", data: urls.join("\n")}]];

    let list = yield Downloads.getList(Downloads.ALL);

    let added = new Set();
    let succeeded = new Set();
    yield new Promise(function(resolve) {
      let view = {
        onDownloadAdded: function(download) {
          added.add(download.source.url);
        },
        onDownloadChanged: function(download) {
          if (!added.has(download.source.url))
            return;
          if (!download.succeeded)
            return;
          succeeded.add(download.source.url);
          if (succeeded.size == urls.length) {
            list.removeView(view).then(resolve);
          }
        }
      };
      list.addView(view).then(function() {
        EventUtils.synthesizeDrop(downloadButton, downloadButton, dragData, "link", window);
      });
    });

    for (let url of urls) {
      ok(added.has(url), url + " is added to download");
    }
  }

  // Ensure that state is reset in case previous tests didn't finish.
  yield task_resetState();

  yield setDownloadDir();

  startServer();

  yield* task_drop([httpUrl("file1.txt")]);
  yield* task_drop([httpUrl("file1.txt"),
                    httpUrl("file2.txt"),
                    httpUrl("file3.txt")]);
});
