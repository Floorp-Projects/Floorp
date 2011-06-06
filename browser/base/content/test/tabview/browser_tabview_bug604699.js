/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let url = "http://www.example.com/";
  let cw;
  let tab = gBrowser.tabs[0];

  let finishTest = function () {
    is(1, gBrowser.tabs.length, "there is one tab, only");
    ok(!TabView.isVisible(), "tabview is not visible");
    finish();
  }

  waitForExplicitFinish();

  let testErroneousLoading = function () {
    cw.ThumbnailStorage.loadThumbnail(tab, url, function (status, data) {
      ok(!status, "thumbnail entry failed to load");
      is(null, data, "no thumbnail data received");
      next();
    });
  }

  let testAsynchronousSaving = function () {
    let saved = false;
    let data = "thumbnail-data-asynchronous";

    cw.ThumbnailStorage.saveThumbnail(tab, data, function (status) {
      ok(status, "thumbnail entry was saved");
      ok(saved, "thumbnail was saved asynchronously");

      cw.ThumbnailStorage.loadThumbnail(tab, url, function (status, imageData) {
        ok(status, "thumbnail entry was loaded");
        is(imageData, data, "valid thumbnail data received");
        next();
      });
    });

    saved = true;
  }

  let testSynchronousSaving = function () {
    let saved = false;
    let data = "thumbnail-data-synchronous";

    cw.UI.isDOMWindowClosing = true;
    registerCleanupFunction(function () cw.UI.isDOMWindowClosing = false);

    cw.ThumbnailStorage.saveThumbnail(tab, data, function (status) {
      ok(status, "thumbnail entry was saved");
      ok(!saved, "thumbnail was saved synchronously");

      cw.ThumbnailStorage.loadThumbnail(tab, url, function (status, imageData) {
        ok(status, "thumbnail entry was loaded");
        is(imageData, data, "valid thumbnail data received");

        cw.UI.isDOMWindowClosing = false;
        next();
      });
    });

    saved = true;
  }

  let tests = [testErroneousLoading, testAsynchronousSaving, testSynchronousSaving];

  let next = function () {
    let test = tests.shift();
    if (test)
      test();
    else
      hideTabView(finishTest);
  }

  tab.linkedBrowser.loadURI(url);
  afterAllTabsLoaded(function() {
    showTabView(function () {
      registerCleanupFunction(function () TabView.hide());
      cw = TabView.getContentWindow();

      next();
    });
  });
}
