/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("destroy");
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: this.window is null");

// Test that projecteditor can be destroyed in various states of loading
// without causing any leaks or exceptions.

add_task(function* () {

  info("Testing tab closure when projecteditor is in various states");
  let loaderUrl = "chrome://devtools/content/projecteditor/chrome/content/projecteditor-test.xul";

  yield addTab(loaderUrl).then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok(iframe, "Tab has placeholder iframe for projecteditor");

    info("Closing the tab without doing anything");
    gBrowser.removeCurrentTab();
  });

  yield addTab(loaderUrl).then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok(iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok(projecteditor, "ProjectEditor has been initialized");

    info("Closing the tab before attempting to load");
    gBrowser.removeCurrentTab();
  });

  yield addTab(loaderUrl).then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok(iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok(projecteditor, "ProjectEditor has been initialized");

    projecteditor.load(iframe);

    info("Closing the tab after a load is requested, but before load is finished");
    gBrowser.removeCurrentTab();
  });

  yield addTab(loaderUrl).then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok(iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok(projecteditor, "ProjectEditor has been initialized");

    return projecteditor.load(iframe).then(() => {
      info("Closing the tab after a load has been requested and finished");
      gBrowser.removeCurrentTab();
    });
  });

  yield addTab(loaderUrl).then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok(iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor(iframe);
    ok(projecteditor, "ProjectEditor has been initialized");

    let loadedDone = promise.defer();
    projecteditor.loaded.then(() => {
      ok(false, "Loaded has finished after destroy() has been called");
      loadedDone.resolve();
    }, () => {
      ok(true, "Loaded has been rejected after destroy() has been called");
      loadedDone.resolve();
    });

    projecteditor.destroy();

    return loadedDone.promise.then(() => {
      gBrowser.removeCurrentTab();
    });
  });

  finish();
});


