/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter is correctly displayed over a variety of elements

waitForExplicitFinish();

let test = asyncTest(function*() {
  info("Adding the test tab and creating the document");
  yield addTab("data:text/html;charset=utf-8,browser_inspector_highlighter.js");
  createDocument(content.document);

  info("Opening the inspector");
  let {toolbox, inspector} = yield openInspector();

  info("Selecting the simple, non-transformed DIV");
  let div = getNode(".simple-div");
  yield selectNode(div, inspector, "highlight");

  testSimpleDivHighlighted(div);
  yield zoomTo(2);
  testZoomedSimpleDivHighlighted(div);
  yield zoomTo(1);

  info("Selecting the rotated DIV");
  let rotated = getNode(".rotated-div");
  let onBoxModelUpdate = waitForBoxModelUpdate();
  yield selectNode(rotated, inspector, "highlight");
  yield onBoxModelUpdate;

  testMouseOverRotatedHighlights(rotated);

  gBrowser.removeCurrentTab();
});

function createDocument(doc) {
  info("Creating the test document");

  let div = doc.createElement("div");
  div.className = "simple-div";
  div.setAttribute("style",
                   "padding:5px; border:7px solid red; margin: 9px; " +
                   "position:absolute; top:30px; left:150px;");
  div.appendChild(doc.createTextNode("Gort! Klaatu barada nikto!"));
  doc.body.appendChild(div);

  let rotatedDiv = doc.createElement("div");
  rotatedDiv.className = "rotated-div";
  rotatedDiv.setAttribute("style",
                       "padding:5px; border:7px solid red; margin: 9px; " +
                       "transform:rotate(45deg); " +
                       "position:absolute; top:30px; left:80px;");
  doc.body.appendChild(rotatedDiv);
}

function testSimpleDivHighlighted(div) {
  ok(isHighlighting(), "The highlighter is shown");
  is(getHighlitNode(), div, "The highlighter's outline corresponds to the simple div");

  info("Checking that the simple div is correctly highlighted");
  isNodeCorrectlyHighlighted(div, "non-zoomed");
}

function testZoomedSimpleDivHighlighted(div) {
  info("Checking that the simple div is correctly highlighted, " +
    "even when the page is zoomed");
  isNodeCorrectlyHighlighted(div, "zoomed");
}

function zoomTo(level) {
  info("Zooming page to " + level);
  let def = promise.defer();

  waitForBoxModelUpdate().then(def.resolve);
  let contentViewer = gBrowser.selectedBrowser.docShell.contentViewer
                              .QueryInterface(Ci.nsIMarkupDocumentViewer);
  contentViewer.fullZoom = level;

  return def.promise;
}

function testMouseOverRotatedHighlights(rotated) {
  ok(isHighlighting(), "The highlighter is shown");
  info("Checking that the rotated div is correctly highlighted");
  isNodeCorrectlyHighlighted(rotated, "rotated");
}
