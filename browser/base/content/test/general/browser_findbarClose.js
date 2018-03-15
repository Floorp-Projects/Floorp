/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests find bar auto-close behavior

var newTab;

add_task(async function findbar_test() {
  waitForExplicitFinish();
  newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let promise = ContentTask.spawn(newTab.linkedBrowser, null, async function() {
    await ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", false);
  });
  newTab.linkedBrowser.loadURI("http://example.com/browser/" +
    "browser/base/content/test/general/test_bug628179.html");
  await promise;

  gFindBar.open();

  await new ContentTask.spawn(newTab.linkedBrowser, null, async function() {
    let iframe = content.document.getElementById("iframe");
    let awaitLoad = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = "http://example.org/";
    await awaitLoad;
  });

  ok(!gFindBar.hidden, "the Find bar isn't hidden after the location of a " +
     "subdocument changes");

  gFindBar.close();
  gBrowser.removeTab(newTab);
  finish();
});

