/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let url = "http://non.existant/url";
  let cw;

  let finishTest = function () {
    is(1, gBrowser.tabs.length, "there is one tab, only");
    ok(!TabView.isVisible(), "tabview is not visible");
    finish();
  }

  waitForExplicitFinish();

  let testErroneousLoading = function () {
    cw.Storage.loadThumbnail(url, function (status, data) {
      ok(!status, "thumbnail entry failed to load");
      is(null, data, "no thumbnail data received");
      next();
    });
  }

  let testAsynchronousSaving = function () {
    let saved = false;
    let data = "thumbnail-data-asynchronous";

    cw.Storage.saveThumbnail(url, data, function (status) {
      ok(status, "thumbnail entry was saved");
      ok(saved, "thumbnail was saved asynchronously");

      cw.Storage.loadThumbnail(url, function (status, imageData) {
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

    cw.Storage.saveThumbnail(url, data, function (status) {
      ok(status, "thumbnail entry was saved");
      ok(!saved, "thumbnail was saved synchronously");

      cw.Storage.loadThumbnail(url, function (status, imageData) {
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

  showTabView(function () {
    registerCleanupFunction(function () TabView.hide());
    cw = TabView.getContentWindow();

    next();
  });
}
