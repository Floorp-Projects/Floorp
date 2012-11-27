/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

let pseudos = [":hover", ":active", ":focus"];

let doc;
let div;
let menu;

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,pseudo-class lock node menu tests";
}

function createDocument()
{
  div = doc.createElement("div");
  div.textContent = "test div";

  doc.body.appendChild(div);

  setupTests();
}

function setupTests()
{
  Services.obs.addObserver(selectNode,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function selectNode()
{
  Services.obs.removeObserver(selectNode,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

  executeSoon(function() {
    InspectorUI.highlighter.addListener("nodeselected", performTests);
    InspectorUI.inspectNode(div);
  });
}

function performTests()
{
  InspectorUI.highlighter.removeListener("nodeselected", performTests);

  menu = document.getElementById("highlighter-node-menu");
  menu.addEventListener("popupshowing", testMenuItems, true);

  menu.openPopup();
}

function testMenuItems()
{
  menu.removeEventListener("popupshowing", testMenuItems, true);

  for each (let pseudo in pseudos) {
    let menuitem = document.getElementById("highlighter-pseudo-class-menuitem-"
                   + pseudo);
    ok(menuitem, pseudo + " menuitem exists");

    menuitem.doCommand();

    is(DOMUtils.hasPseudoClassLock(div, pseudo), true,
       "pseudo-class lock has been applied");
  }
  finishUp();
}

function finishUp()
{
  InspectorUI.closeInspectorUI();
  doc = div = null;
  gBrowser.removeCurrentTab();
  finish();
}
