/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/inspector/" +
                 "test/doc_inspector_infobar.html";
const DOORHANGER_ARROW_HEIGHT = 5;

// Test that hovering over nodes in the markup-view shows the highlighter over
// those nodes
let test = asyncTest(function*() {
  info("Loading the test document and opening the inspector");

  yield addTab(TEST_URI);

  let {inspector} = yield openInspector();

  let doc = content.document;
  let testData = [
    {
      node: doc.querySelector("#top"),
      position: "bottom",
      tag: "DIV",
      id: "#top",
      classes: ".class1.class2",
      dims: "500 x 100"
    },
    {
      node: doc.querySelector("#vertical"),
      position: "overlap",
      tag: "DIV",
      id: "#vertical",
      classes: ""
      // No dims as they will vary between computers
    },
    {
      node: doc.querySelector("#bottom"),
      position: "top",
      tag: "DIV",
      id: "#bottom",
      classes: "",
      dims: "500 x 100"
    },
    {
      node: doc.querySelector("body"),
      position: "bottom",
      tag: "BODY",
      id: "",
      classes: ""
      // No dims as they will vary between computers
    },
    {
      node: doc.querySelector("#farbottom"),
      position: "top",
      tag: "DIV",
      id: "#farbottom",
      classes: "",
      dims: "500 x 100"
    },
  ];

  for (let currTest of testData) {
    yield testPosition(currTest, inspector);
  }

  yield checkInfoBarAboveTop(inspector);
  yield checkInfoBarBelowFindbar(inspector);

  gBrowser.removeCurrentTab();
});

function* testPosition(currTest, inspector) {
  let browser = gBrowser.selectedBrowser;
  let stack = browser.parentNode;

  info("Testing " + currTest.id);

  yield selectNode(currTest.node, inspector, "highlight");

  let container = stack.querySelector(".highlighter-nodeinfobar-positioner");
  is(container.getAttribute("position"),
    currTest.position, "node " + currTest.id + ": position matches.");

  let tagNameLabel = stack.querySelector(".highlighter-nodeinfobar-tagname");
  is(tagNameLabel.textContent, currTest.tag,
    "node " + currTest.id + ": tagName matches.");

  if (currTest.id) {
    let idLabel = stack.querySelector(".highlighter-nodeinfobar-id");
    is(idLabel.textContent, currTest.id, "node " + currTest.id  + ": id matches.");
  }

  let classesBox = stack.querySelector(".highlighter-nodeinfobar-classes");
  is(classesBox.textContent, currTest.classes,
    "node " + currTest.id  + ": classes match.");

  if (currTest.dims) {
    let dimBox = stack.querySelector(".highlighter-nodeinfobar-dimensions");
    is(dimBox.textContent, currTest.dims, "node " + currTest.id  + ": dims match.");
  }
}

function* checkInfoBarAboveTop(inspector) {
  yield selectNode("#abovetop", inspector);

  let positioner = getPositioner();
  let insideContent = parseInt(positioner.style.top, 10) >= -DOORHANGER_ARROW_HEIGHT;

  ok(insideContent, "Infobar is inside the content window (top = " +
                    parseInt(positioner.style.top, 10) + ", content = '" +
                    positioner.textContent +"')");
}

function* checkInfoBarBelowFindbar(inspector) {
  gFindBar.open();

  let body = content.document.body;
  let farBottom = body.querySelector("#farbottom");
  farBottom.scrollIntoView();

  // Wait for scrollIntoView
  yield waitForTick();

  body.scrollTop -= 130;
  yield selectNode(farBottom, inspector);

  let positioner = getPositioner();
  let insideContent = parseInt(positioner.style.top, 10) >= -DOORHANGER_ARROW_HEIGHT;

  ok(insideContent, "Infobar does not overlap the findbar (top = " +
                    parseInt(positioner.style.top, 10) + ", content = '" +
                    positioner.textContent +"')");

  gFindBar.close();
}

function getPositioner() {
  let browser = gBrowser.selectedBrowser;
  let stack = browser.parentNode;

  return stack.querySelector(".highlighter-nodeinfobar-positioner");
}

function waitForTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}
