/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetter(this, "HttpServer",
  "resource://testing-common/httpd.js");

registerCleanupFunction(async function() {
  await task_resetState();
  await PlacesUtils.history.clear();
});

add_task(async function test_indicatorDrop() {
  let downloadButton = document.getElementById("downloads-button");
  ok(downloadButton, "download button present");

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
      getService(Ci.mozIJSSubScriptLoader);
  let EventUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  async function task_drop(urls) {
    let dragData = [[{type: "text/plain", data: urls.join("\n")}]];

    let list = await Downloads.getList(Downloads.ALL);

    let added = new Set();
    let succeeded = new Set();
    await new Promise(function(resolve) {
      let view = {
        onDownloadAdded(download) {
          added.add(download.source.url);
        },
        onDownloadChanged(download) {
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
  await task_resetState();

  await setDownloadDir();

  startServer();

  await task_drop([httpUrl("file1.txt")]);
  await task_drop([httpUrl("file1.txt"),
                    httpUrl("file2.txt"),
                    httpUrl("file3.txt")]);
});
