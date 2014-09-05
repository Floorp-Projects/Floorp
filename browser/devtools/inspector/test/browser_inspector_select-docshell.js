/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test frame selection switching at toolbox level
// when using the inspector

let test = asyncTest(function*() {
  const FrameURL = "data:text/html;charset=UTF-8," + encodeURI("<div id=\"frame\">frame</div>");
  const URL = "data:text/html;charset=UTF-8," + encodeURI("<iframe src=\"" + FrameURL + "\"></iframe><div id=\"top\">top</div>");

  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);

  let {toolbox, inspector} = yield openInspectorForURL(URL);

  // Verify we are on the top level document
  let testNode = content.document.querySelector("#top");
  ok(testNode, "We have the test node on the top level document");

  assertMarkupViewIsLoaded();

  // Verify that the frame list button is visible and populated
  let btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.firstChild.getAttribute("hidden"), "The frame list button is visible");
  let frameBtns = Array.slice(btn.firstChild.querySelectorAll("[data-window-id]"));
  is(frameBtns.length, 2, "We have both frames in the list");
  frameBtns.sort(function (a, b) {
    return a.getAttribute("label").localeCompare(b.getAttribute("label"));
  });
  is(frameBtns[0].getAttribute("label"), FrameURL, "Got top level document in the list");
  is(frameBtns[1].getAttribute("label"), URL, "Got iframe document in the list");

  // Listen to will-navigate to check if the view is empty
  let willNavigate = toolbox.target.on("will-navigate", () => {
    info("Navigation to the iframe has started, the inspector should be empty");
    assertMarkupViewIsEmpty();
  });
  let newRoot = inspector.once("new-root", () => {
    info("Navigation to the iframe is done, the inspector should be back up");

    // Verify we are on page one
    //let testNode = content.frames[0].document.querySelector("#frame");
    let testNode = getNode("#frame", { document: content.frames[0].document});
    ok(testNode, "We have the test node on the iframe");

    // On page 2 load, verify we have the right content
    assertMarkupViewIsLoaded();

    inspector.once("inspector-updated", () => {
      deferred.resolve();
    });
    selectNode(testNode, inspector);
  });

  // select the iframe once we were able to select an element from the
  // top level document
  selectNode("#top", inspector);
  inspector.once("inspector-updated", () => {
    // Select the iframe
    frameBtns[0].click();
  });

  yield willNavigate;
  yield newRoot;

//  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("devtools.command-button-frames.enabled");

  function assertMarkupViewIsLoaded() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
  }

  function assertMarkupViewIsEmpty() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 0, "The markup-view is unloaded");
  }
});

