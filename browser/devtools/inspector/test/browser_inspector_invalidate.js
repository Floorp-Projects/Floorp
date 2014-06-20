/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let doc;
  let div;
  let inspector;

  function createDocument() {
    div = doc.createElement("div");
    div.setAttribute("style", "width: 100px; height: 100px; background:yellow;");
    doc.body.appendChild(div);

    openInspector(aInspector => {
      inspector = aInspector;
      inspector.toolbox.highlighter.showBoxModel(getNodeFront(div)).then(runTest);
    });
  }

  function runTest() {
    info("Checking that the highlighter has the right size");
    let rect = getSimpleBorderRect();
    is(rect.width, 100, "outline has the right width");

    waitForBoxModelUpdate().then(testRectWidth);

    info("Changing the test element's size");
    div.style.width = "200px";
  }

  function testRectWidth() {
    info("Checking that the highlighter has the right size after update");
    let rect = getSimpleBorderRect();
    is(rect.width, 200, "outline updated");
    finishUp();
  }

  function finishUp() {
    inspector.toolbox.highlighter.hideBoxModel().then(() => {
      doc = div = inspector = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  }

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_inspector_invalidate.js";
}
