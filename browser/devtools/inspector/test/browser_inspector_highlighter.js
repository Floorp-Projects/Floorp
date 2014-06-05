/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let div;
let rotated;
let inspector;
let contentViewer;

function createDocument() {
  div = doc.createElement("div");
  div.setAttribute("style",
                   "padding:5px; border:7px solid red; margin: 9px; " +
                   "position:absolute; top:30px; left:150px;");
  let textNode = doc.createTextNode("Gort! Klaatu barada nikto!");
  rotated = doc.createElement("div");
  rotated.setAttribute("style",
                       "padding:5px; border:7px solid red; margin: 9px; " +
                       "transform:rotate(45deg); " +
                       "position:absolute; top:30px; left:80px;");
  div.appendChild(textNode);
  doc.body.appendChild(div);
  doc.body.appendChild(rotated);

  openInspector(aInspector => {
    inspector = aInspector;
    inspector.selection.setNode(div, null);
    inspector.once("inspector-updated", testMouseOverDivHighlights);
  });
}

function testMouseOverDivHighlights() {
  ok(isHighlighting(), "Highlighter is shown");
  is(getHighlitNode(), div, "Highlighter's outline correspond to the non-rotated div");
  testNonTransformedBoxModelDimensionsNoZoom();
}

function testNonTransformedBoxModelDimensionsNoZoom() {
  info("Highlighted the non-rotated div");
  isNodeCorrectlyHighlighted(div, "non-zoomed");

  inspector.toolbox.once("highlighter-ready", testNonTransformedBoxModelDimensionsZoomed);
  contentViewer = gBrowser.selectedBrowser.docShell.contentViewer
                          .QueryInterface(Ci.nsIMarkupDocumentViewer);
  contentViewer.fullZoom = 2;
}

function testNonTransformedBoxModelDimensionsZoomed() {
  info("Highlighted the zoomed, non-rotated div");
  isNodeCorrectlyHighlighted(div, "zoomed");

  inspector.toolbox.once("highlighter-ready", testMouseOverRotatedHighlights);
  contentViewer.fullZoom = 1;
}

function testMouseOverRotatedHighlights() {
  inspector.toolbox.once("highlighter-ready", () => {
    ok(isHighlighting(), "Highlighter is shown");
    info("Highlighted the rotated div");
    isNodeCorrectlyHighlighted(rotated, "rotated");

    executeSoon(finishUp);
  });
  inspector.selection.setNode(rotated);
}

function finishUp() {
  inspector.toolbox.highlighterUtils.stopPicker().then(() => {
    doc = div = rotated = inspector = contentViewer = null;
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
