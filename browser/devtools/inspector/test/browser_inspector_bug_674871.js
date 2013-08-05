/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  let doc;
  let iframeNode, iframeBodyNode;

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
    openInspector(runTests);
  }

  function runTests(inspector)
  {
    inspector.highlighter.unlock();
    executeSoon(function() {
      inspector.highlighter.once("highlighting", isTheIframeSelected);
      moveMouseOver(iframeNode, 1, 1);
    });
  }

  function isTheIframeSelected()
  {
    let inspector = getActiveInspector();

    is(inspector.selection.node, iframeNode, "selection matches node");
    iframeNode.style.marginBottom = doc.defaultView.innerHeight + "px";
    doc.defaultView.scrollBy(0, 40);

    executeSoon(function() {
      inspector.selection.once("new-node", isTheIframeContentSelected);
      moveMouseOver(iframeNode, 40, 40);
    });
  }

  function isTheIframeContentSelected()
  {
    let inspector = getActiveInspector();
    is(inspector.selection.node, iframeBodyNode, "selection matches node");
    // 184 == 200 + 11(border) + 13(padding) - 40(scroll)
    is(inspector.highlighter._highlightRect.height, 184,
      "highlighter height");

    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
    finishUp();
  }

  function finishUp() {
    doc = iframeNode = iframeBodyNode = null;
    gBrowser.removeCurrentTab();
    finish();
  }


  function moveMouseOver(aElement, x, y)
  {
    EventUtils.synthesizeMouse(aElement, x, y, {type: "mousemove"},
                               aElement.ownerDocument.defaultView);
  }

}

