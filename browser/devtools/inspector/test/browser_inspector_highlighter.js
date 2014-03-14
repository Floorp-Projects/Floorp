/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let h1;
let inspector;

function createDocument() {
  let div = doc.createElement("div");
  h1 = doc.createElement("h1");
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

  openInspector(aInspector => {
    inspector = aInspector;
    inspector.selection.setNode(div, null);
    inspector.once("inspector-updated", () => {
      inspector.toolbox.highlighterUtils.startPicker().then(testMouseOverH1Highlights);
    });
  });
}

function testMouseOverH1Highlights() {
  inspector.toolbox.once("highlighter-ready", () => {
    ok(isHighlighting(), "Highlighter is shown");
    is(getHighlitNode(), h1, "Highlighter's outline correspond to the selected node");
    testBoxModelDimensions();
  });

  EventUtils.synthesizeMouse(h1, 2, 2, {type: "mousemove"}, content);
}

function testBoxModelDimensions() {
  let h1Dims = h1.getBoundingClientRect();
  let h1Width = Math.ceil(h1Dims.width);
  let h1Height = Math.ceil(h1Dims.height);

  let outlineDims = getSimpleBorderRect();
  let outlineWidth = Math.ceil(outlineDims.width);
  let outlineHeight = Math.ceil(outlineDims.height);

  // Disabled due to bug 716245
  is(outlineWidth, h1Width, "outline width matches dimensions of element (no zoom)");
  is(outlineHeight, h1Height, "outline height matches dimensions of element (no zoom)");

  // zoom the page by a factor of 2
  let contentViewer = gBrowser.selectedBrowser.docShell.contentViewer
                             .QueryInterface(Ci.nsIMarkupDocumentViewer);
  contentViewer.fullZoom = 2;

  // simulate the zoomed dimensions of the div element
  let h1Dims = h1.getBoundingClientRect();
  // There seems to be some very minor differences in the floats, so let's
  // floor the values
  let h1Width = Math.floor(h1Dims.width * contentViewer.fullZoom);
  let h1Height = Math.floor(h1Dims.height * contentViewer.fullZoom);

  let outlineDims = getSimpleBorderRect();
  let outlineWidth = Math.floor(outlineDims.width);
  let outlineHeight = Math.floor(outlineDims.height);

  is(outlineWidth, h1Width, "outline width matches dimensions of element (zoomed)");

  is(outlineHeight, h1Height, "outline height matches dimensions of element (zoomed)");

  executeSoon(finishUp);
}

function finishUp() {
  inspector.toolbox.highlighterUtils.stopPicker().then(() => {
    doc = h1 = inspector = null;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
    gBrowser.removeCurrentTab();
    finish();
  });
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_inspector_highlighter.js";
}
