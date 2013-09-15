/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let modifiers = {
  accelKey: true
};

let toolbox;

function test() {
  waitForExplicitFinish();

  addTab("about:blank", function() {
    openToolbox();
  });
}

function openToolbox() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target).then((aToolbox) => {
    toolbox = aToolbox;
    toolbox.selectTool("styleeditor").then(testZoom);
  });
}

function testZoom() {
  info("testing zoom keys");

  testZoomLevel("in", 2, 1.2);
  testZoomLevel("out", 3, 0.9);
  testZoomLevel("reset", 1, 1);

  tidyUp();
}

function testZoomLevel(type, times, expected) {
  sendZoomKey("toolbox-zoom-"+ type + "-key", times);

  let zoom = getCurrentZoom(toolbox);
  is(zoom.toFixed(2), expected, "zoom level correct after zoom " + type);

  is(toolbox.zoomValue.toFixed(2), expected,
     "saved zoom level is correct after zoom " + type);
}

function sendZoomKey(id, times) {
  let key = toolbox.doc.getElementById(id).getAttribute("key");
  for (let i = 0; i < times; i++) {
    EventUtils.synthesizeKey(key, modifiers, toolbox.doc.defaultView);
  }
}

function getCurrentZoom() {
  var contViewer = toolbox.frame.docShell.contentViewer;
  var docViewer = contViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
  return docViewer.fullZoom;
}

function tidyUp() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();

    toolbox = modifiers = null;
    finish();
  });
}
