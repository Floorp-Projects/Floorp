/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let div;

function createDocument()
{
  div = doc.createElement("div");
  div.setAttribute("style", "width: 100px; height: 100px; background:yellow;");
  doc.body.appendChild(div);

  Services.obs.addObserver(runTest,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function runTest(subject)
{
  Services.obs.removeObserver(runTest,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  InspectorUI.highlighter.highlight(div);

  executeSoon(function() {
    let outline = InspectorUI.highlighter.outline;
    is(outline.style.width, "100px", "selection has the right width");

    div.style.width = "200px";
    function pollTest() {
      if (outline.style.width == "100px") {
    	setTimeout(pollTest, 10);
    	return;
      }
      is(outline.style.width, "200px", "selection updated");
      InspectorUI.closeInspectorUI();
      gBrowser.removeCurrentTab();
      finish();
    }
    setTimeout(pollTest, 10);
  });
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,basic tests for inspector";
}

