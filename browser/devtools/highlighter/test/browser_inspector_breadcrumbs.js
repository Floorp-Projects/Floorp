/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  let nodes = [
    {nodeId: "i1111", result: "i1 i11 i111 i1111"},
    {nodeId: "i22", result: "i2 i22 i221"},
    {nodeId: "i2111", result: "i2 i21 i211 i2111"},
    {nodeId: "i21", result: "i2 i21 i211 i2111"},
    {nodeId: "i22211", result: "i2 i22 i222 i2221 i22211"},
    {nodeId: "i22", result: "i2 i22 i222 i2221 i22211"},
  ];

  let doc;
  let nodes;
  let cursor;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/highlighter/test/browser_inspector_breadcrumbs.html";

  function setupTest()
  {
    for (let i = 0; i < nodes.length; i++) {
      let node = doc.getElementById(nodes[i].nodeId);
      nodes[i].node = node;
      ok(nodes[i].node, "node " + nodes[i].nodeId + " found");
    }

    Services.obs.addObserver(runTests,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests()
  {
    Services.obs.removeObserver(runTests,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

    cursor = 0;
    executeSoon(function() {
      Services.obs.addObserver(nodeSelected,
        InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

      InspectorUI.inspectNode(nodes[0].node);
    });
  }

  function nodeSelected()
  {
    executeSoon(function() {
      performTest();
      cursor++;
      if (cursor >= nodes.length) {

        Services.obs.removeObserver(nodeSelected,
          InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
        Services.obs.addObserver(finishUp,
          InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

        executeSoon(function() {
          InspectorUI.closeInspectorUI();
        });
      } else {
        let node = nodes[cursor].node;
        InspectorUI.inspectNode(node);
      }
    });
  }

  function performTest()
  {
    let container = document.getElementById("inspector-breadcrumbs");
    let buttonsLabelIds = nodes[cursor].result.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2, "Node " + cursor + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      let expectedId = "#" + buttonsLabelIds[i - 2];
      let button = container.childNodes[i];
      let labelId = button.querySelector(".inspector-breadcrumbs-id");
      is(labelId.textContent, expectedId, "Node " + cursor + ": button " + i + " matches");
    }

    let checkedButton = container.querySelector("button[checked]");
    let labelId = checkedButton.querySelector(".inspector-breadcrumbs-id");
    is(labelId.textContent, "#" + InspectorUI.selection.id, "Node " + cursor + ": selection matches");
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = nodes = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
