/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  let doc;
  let nodes;
  let cursor;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupInfobarTest, content);
  }, true);

  let style = "body{width:100%;height: 100%} div {position: absolute;height: 100px;width: 500px}#bottom {bottom: 0px}#vertical {height: 100%}#farbottom{bottom: -200px}";
  let html = "<style>" + style + "</style><div id=vertical></div><div id=top class='class1 class2'></div><div id=bottom></div><div id=farbottom></div>"

  content.location = "data:text/html," + encodeURIComponent(html);

  function setupInfobarTest()
  {
    nodes = [
      {node: doc.querySelector("#top"), position: "bottom", tag: "DIV", id: "#top", classes: ".class1.class2"},
      {node: doc.querySelector("#vertical"), position: "overlap", tag: "DIV", id: "#vertical", classes: ""},
      {node: doc.querySelector("#bottom"), position: "top", tag: "DIV", id: "#bottom", classes: ""},
      {node: doc.querySelector("body"), position: "overlap", tag: "BODY", id: "", classes: ""},
      {node: doc.querySelector("#farbottom"), position: "top", tag: "DIV", id: "#farbottom", classes: ""},
    ]

    for (let i = 0; i < nodes.length; i++) {
      ok(nodes[i].node, "node " + i + " found");
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
      InspectorUI.inspectNode(nodes[0].node);
      nodeSelected();
    });
  }

  function nodeSelected()
  {
    executeSoon(function() {
      performTest();
      cursor++;
      if (cursor >= nodes.length) {

        Services.obs.addObserver(finishUp,
          InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

        executeSoon(function() {
          InspectorUI.closeInspectorUI();
        });
      } else {
        let node = nodes[cursor].node;
        InspectorUI.inspectNode(node);
        nodeSelected();
      }
    });
  }

  function performTest()
  {
    let container = document.getElementById("highlighter-nodeinfobar-container");
    is(container.getAttribute("position"), nodes[cursor].position, "node " + cursor + ": position matches.");

    let tagNameLabel = document.getElementById("highlighter-nodeinfobar-tagname");
    is(tagNameLabel.textContent, nodes[cursor].tag, "node " + cursor  + ": tagName matches.");

    let idLabel = document.getElementById("highlighter-nodeinfobar-id");
    is(idLabel.textContent, nodes[cursor].id, "node " + cursor  + ": id matches.");

    let classesBox = document.getElementById("highlighter-nodeinfobar-classes");
    is(classesBox.textContent, nodes[cursor].classes, "node " + cursor  + ": classes match.");
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = nodes = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}

