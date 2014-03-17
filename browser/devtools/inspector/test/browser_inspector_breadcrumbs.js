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
  let inspector;
  let target;
  let panel;
  let container;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_breadcrumbs.html";

  function setupTest()
  {
    for (let i = 0; i < nodes.length; i++) {
      let node = doc.getElementById(nodes[i].nodeId);
      nodes[i].node = node;
      ok(nodes[i].node, "node " + nodes[i].nodeId + " found");
    }

    openInspector(runTests);
  }

  function runTests(aInspector)
  {
    inspector = aInspector;
    target = TargetFactory.forTab(gBrowser.selectedTab);
    panel = gDevTools.getToolbox(target).getPanel("inspector");
    container = panel.panelDoc.getElementById("inspector-breadcrumbs");
    cursor = 0;
    inspector.on("breadcrumbs-updated", nodeSelected);
    executeSoon(function() {
      inspector.selection.setNode(nodes[0].node);
    });
  }

  function nodeSelected()
  {
    performTest();
    cursor++;

    if (cursor >= nodes.length) {
      inspector.off("breadcrumbs-updated", nodeSelected);
      // breadcrumbs-updated is an event that is fired before the rest of the
      // inspector is updated, so there'll be hanging connections if we finish
      // up before waiting for everything to end.
      inspector.once("inspector-updated", finishUp);
    } else {
      let node = nodes[cursor].node;
      inspector.selection.setNode(node);
    }
  }

  function performTest()
  {
    let buttonsLabelIds = nodes[cursor].result.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2, "Node " + cursor + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      let expectedId = "#" + buttonsLabelIds[i - 2];
      let button = container.childNodes[i];
      let labelId = button.querySelector(".breadcrumbs-widget-item-id");
      is(labelId.textContent, expectedId, "Node " + cursor + ": button " + i + " matches");
    }

    let checkedButton = container.querySelector("button[checked]");
    let labelId = checkedButton.querySelector(".breadcrumbs-widget-item-id");
    let id = inspector.selection.node.id;
    is(labelId.textContent, "#" + id, "Node " + cursor + ": selection matches");
  }

  function finishUp() {
    doc = nodes = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
