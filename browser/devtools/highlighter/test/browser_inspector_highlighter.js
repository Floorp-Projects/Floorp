/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let h1;
let div;

function createDocument()
{
  let div = doc.createElement("div");
  let h1 = doc.createElement("h1");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  let div2 = doc.createElement("div");
  let p3 = doc.createElement("p");
  doc.title = "Inspector Highlighter Meatballs";
  h1.textContent = "Inspector Tree Selection Test";
  p1.textContent = "This is some example text";
  p2.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  p3.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  let div3 = doc.createElement("div");
  div3.id = "checkOutThisWickedSpread";
  div3.setAttribute("style", "position: absolute; top: 20px; right: 20px; height: 20px; width: 20px; background-color: yellow; border: 1px dashed black;");
  let p4 = doc.createElement("p");
  p4.setAttribute("style", "font-weight: 200; font-size: 8px; text-align: center;");
  p4.textContent = "Smörgåsbord!";
  div.appendChild(h1);
  div.appendChild(p1);
  div.appendChild(p2);
  div2.appendChild(p3);
  div3.appendChild(p4);
  doc.body.appendChild(div);
  doc.body.appendChild(div2);
  doc.body.appendChild(div3);

  setupHighlighterTests();
}

function setupHighlighterTests()
{
  h1 = doc.querySelector("h1");
  ok(h1, "we have the header");
  Services.obs.addObserver(runSelectionTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function runSelectionTests(subject)
{
  Services.obs.removeObserver(runSelectionTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  is(subject.wrappedJSObject, InspectorUI,
     "InspectorUI accessible in the observer");

  InspectorUI.highlighter.veilContainer.setAttribute("disable-transitions", "true");

  executeSoon(function() {
    InspectorUI.highlighter.addListener("nodeselected", performTestComparisons);
    EventUtils.synthesizeMouse(h1, 2, 2, {type: "mousemove"}, content);
  });
}

function performTestComparisons(evt)
{
  InspectorUI.highlighter.removeListener("nodeselected", performTestComparisons);

  InspectorUI.stopInspecting();
  ok(isHighlighting(), "highlighter is highlighting");
  is(getHighlitNode(), h1, "highlighter matches selection")
  is(InspectorUI.selection, h1, "selection matches node");
  is(InspectorUI.selection, getHighlitNode(), "selection matches highlighter");


  div = doc.querySelector("div#checkOutThisWickedSpread");

  executeSoon(function() {
    InspectorUI.highlighter.addListener("nodeselected", finishTestComparisons);
    InspectorUI.inspectNode(div);
  });
}

function finishTestComparisons()
{
  InspectorUI.highlighter.removeListener("nodeselected", finishTestComparisons);

  // get dimensions of div element
  let divDims = div.getBoundingClientRect();
  let divWidth = divDims.width;
  let divHeight = divDims.height;

  // get dimensions of transparent veil box over element
  let veilBoxDims = 
    InspectorUI.highlighter.veilTransparentBox.getBoundingClientRect();
  let veilBoxWidth = veilBoxDims.width;
  let veilBoxHeight = veilBoxDims.height;

  // Disabled due to bug 716245
  //is(veilBoxWidth, divWidth, "transparent veil box width matches dimensions of element (no zoom)");
  //is(veilBoxHeight, divHeight, "transparent veil box height matches dimensions of element (no zoom)");

  // zoom the page by a factor of 2
  let contentViewer = InspectorUI.browser.docShell.contentViewer
                             .QueryInterface(Ci.nsIMarkupDocumentViewer);
  contentViewer.fullZoom = 2;

  executeSoon(function() {
    // check what zoom factor we're at, should be 2
    let zoom = InspectorUI.highlighter.zoom;
    is(zoom, 2, "zoom is 2?");

    // simulate the zoomed dimensions of the div element
    let divDims = div.getBoundingClientRect();
    let divWidth = divDims.width * zoom;
    let divHeight = divDims.height * zoom;

    // now zoomed, get new dimensions of transparent veil box over element
    let veilBoxDims = InspectorUI.highlighter.veilTransparentBox.getBoundingClientRect();
    let veilBoxWidth = veilBoxDims.width;
    let veilBoxHeight = veilBoxDims.height;

    // Disabled due to bug 716245
    //is(veilBoxWidth, divWidth, "transparent veil box width matches width of element (2x zoom)");
    //is(veilBoxHeight, divHeight, "transparent veil box height matches width of element (2x zoom)");

    doc = h1 = div = null;
    executeSoon(finishUp);
  });
}

function finishUp() {
  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  finish();
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

