/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  let doc;
  let iframeNode, iframeBodyNode;
  let inspector;

  let iframeSrc = "<style>" +
                  "body {" +
                  "margin:0;" +
                  "height:100%;" +
                  "background-color:red" +
                  "}" +
                  "</style>" +
                  "<body></body>";
  let docSrc = "<style>" +
               "iframe {" +
               "height:200px;" +
               "border: 11px solid black;" +
               "padding: 13px;" +
               "}" +
               "body,iframe {" +
               "margin:0" +
               "}" +
               "</style>" +
               "<body>" +
               "<iframe src='data:text/html," + iframeSrc + "'></iframe>" +
               "</body>";

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html," + docSrc;

  function setupTest()
  {
    iframeNode = doc.querySelector("iframe");
    iframeBodyNode = iframeNode.contentDocument.querySelector("body");
    ok(iframeNode, "we have the iframe node");
    ok(iframeBodyNode, "we have the body node");
    openInspector(aInspector => {
      inspector = aInspector;
      // Make sure the highlighter is shown so we can disable transitions
      inspector.toolbox.highlighter.showBoxModel(getNodeFront(doc.body)).then(() => {
        runTests();
      });
    });
  }

  function runTests()
  {
    inspector.toolbox.highlighterUtils.startPicker().then(() => {
      moveMouseOver(iframeNode, 1, 1, isTheIframeHighlighted);
    });
  }

  function isTheIframeHighlighted()
  {
    let {p1, p2, p3, p4} = getBoxModelStatus().border.points;
    let {top, right, bottom, left} = iframeNode.getBoundingClientRect();

    is(top, p1.y, "iframeRect.top === boxModelStatus.p1.y");
    is(top, p2.y, "iframeRect.top === boxModelStatus.p2.y");
    is(right, p2.x, "iframeRect.right === boxModelStatus.p2.x");
    is(right, p3.x, "iframeRect.right === boxModelStatus.p3.x");
    is(bottom, p3.y, "iframeRect.bottom === boxModelStatus.p3.y");
    is(bottom, p4.y, "iframeRect.bottom === boxModelStatus.p4.y");
    is(left, p1.x, "iframeRect.left === boxModelStatus.p1.x");
    is(left, p4.x, "iframeRect.left === boxModelStatus.p4.x");

    iframeNode.style.marginBottom = doc.defaultView.innerHeight + "px";
    doc.defaultView.scrollBy(0, 40);

    moveMouseOver(iframeNode, 40, 40, isTheIframeContentHighlighted);
  }

  function isTheIframeContentHighlighted()
  {
    is(getHighlitNode(), iframeBodyNode, "highlighter shows the right node");

    let outlineRect = getSimpleBorderRect();
    is(outlineRect.height, 200, "highlighter height");

    inspector.toolbox.highlighterUtils.stopPicker().then(() => {
      let target = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.closeToolbox(target);
      finishUp();
    });
  }

  function finishUp()
  {
    doc = inspector = iframeNode = iframeBodyNode = null;
    gBrowser.removeCurrentTab();
    finish();
  }

  function moveMouseOver(aElement, x, y, cb)
  {
    inspector.toolbox.once("picker-node-hovered", cb);
    EventUtils.synthesizeMouse(aElement, x, y, {type: "mousemove"},
                               aElement.ownerDocument.defaultView);
  }
}
