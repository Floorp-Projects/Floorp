/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

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
        getHighlighterOutline().setAttribute("disable-transitions", "true");
        runTests();
      });
    });
  }

  function runTests()
  {
    inspector.toolbox.startPicker().then(() => {
      moveMouseOver(iframeNode, 1, 1, isTheIframeHighlighted);
    });
  }

  function isTheIframeHighlighted()
  {
    let outlineRect = getHighlighterOutlineRect();
    let iframeRect = iframeNode.getBoundingClientRect();
    for (let dim of ["width", "height", "top", "left"]) {
      is(Math.floor(outlineRect[dim]), Math.floor(iframeRect[dim]),
         "Outline dimension is correct " + outlineRect[dim]);
    }

    iframeNode.style.marginBottom = doc.defaultView.innerHeight + "px";
    doc.defaultView.scrollBy(0, 40);

    moveMouseOver(iframeNode, 40, 40, isTheIframeContentHighlighted);
  }

  function isTheIframeContentHighlighted()
  {
    is(getHighlitNode(), iframeBodyNode, "highlighter shows the right node");

    // 184 == 200 + 11(border) + 13(padding) - 40(scroll)
    let outlineRect = getHighlighterOutlineRect();
    is(outlineRect.height, 184, "highlighter height");

    inspector.toolbox.stopPicker().then(() => {
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
