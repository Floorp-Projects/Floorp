/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests find bar auto-close behavior

var newTab;

add_task(function* findbar_test() {
  waitForExplicitFinish();
  newTab = gBrowser.addTab("about:blank");

  let promise = ContentTask.spawn(newTab.linkedBrowser, null, function* () {
    yield ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", false);
  });
  newTab.linkedBrowser.loadURI("http://example.com/browser/" +
    "browser/base/content/test/general/test_bug628179.html");
  yield promise;

  gFindBar.open();

  yield new ContentTask.spawn(newTab.linkedBrowser, null, function* () {
    let iframe = content.document.getElementById("iframe");
    let promise = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = "http://example.org/";
    yield promise;
  });

  ok(!gFindBar.hidden, "the Find bar isn't hidden after the location of a " +
     "subdocument changes");

  gFindBar.close();
  gBrowser.removeTab(newTab);
  finish();
});

