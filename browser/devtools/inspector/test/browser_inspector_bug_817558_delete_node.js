/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();
  //ignoreAllUncaughtExceptions();

  let node, iframe, inspector;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_destroyselection.html";

  function setupTest()
  {
    iframe = content.document.querySelector("iframe");
    node = iframe.contentDocument.querySelector("span");
    openInspector(runTests);
  }

  function runTests(aInspector)
  {
    inspector = aInspector;
    inspector.selection.setNode(node);
    inspector.once("inspector-updated", () => {
      let parentNode = node.parentNode;
      parentNode.removeChild(node);

      let tmp = {};
      Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", tmp);
      let lh = new tmp.LayoutHelpers(window.content);
      ok(!lh.isNodeConnected(node), "Node considered as disconnected.");

      // Wait for the inspector to process the mutation
      inspector.once("inspector-updated", () => {
        is(inspector.selection.node, parentNode, "parent of selection got selected");
        finishUp();
      });
    });
  };

  function finishUp() {
    node = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}

