/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();
  //ignoreAllUncaughtExceptions();

  let node, iframe;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/highlighter/test/browser_inspector_destroyselection.html";

  function setupTest()
  {
    iframe = content.document.querySelector("iframe");
    node = iframe.contentDocument.querySelector("span");

    Services.obs.addObserver(runTests,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.openInspectorUI(node);
  }

  function runTests()
  {
    Services.obs.removeObserver(runTests,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
    is(InspectorUI.selection, node, "node selected");
    iframe.parentNode.removeChild(iframe);
    iframe = null;

    let tmp = {};
    Cu.import("resource:///modules/devtools/LayoutHelpers.jsm", tmp);
    ok(!tmp.LayoutHelpers.isNodeConnected(node), "Node considered as disconnected.");

    Services.obs.addObserver(testBreadcrumbs,
                             InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

    executeSoon(function() {
      InspectorUI.closeInspectorUI();
    });
  }

  function testBreadcrumbs()
  {
    Services.obs.removeObserver(testBreadcrumbs, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
    ok(!InspectorUI.breadcrumbs, "Breadcrumbs destroyed");
    finishUp();
  }

  function finishUp() {
    node = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}

