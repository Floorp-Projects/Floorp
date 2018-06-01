/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test frame selection switching at toolbox level
// when using the inspector

const FrameURL = "data:text/html;charset=UTF-8," +
                 encodeURI("<div id=\"frame\">frame</div>");
const URL = "data:text/html;charset=UTF-8," +
            encodeURI('<iframe src="' + FrameURL +
                      '"></iframe><div id="top">top</div>');

add_task(async function() {
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);

  const {inspector, toolbox, testActor} = await openInspectorForURL(URL);

  // Verify we are on the top level document
  ok((await testActor.hasNode("#top")),
     "We have the test node on the top level document");

  assertMarkupViewIsLoaded(inspector);

  // Verify that the frame map button is empty at the moment.
  const btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.firstChild, "The frame list button doesn't have any children");

  // Open frame menu and wait till it's available on the screen.
  const menu = await toolbox.showFramesMenu({target: btn});
  await once(menu, "open");

  // Verify that the menu is popuplated.
  const frames = menu.items.slice();
  is(frames.length, 2, "We have both frames in the menu");

  frames.sort(function(a, b) {
    return a.label.localeCompare(b.label);
  });

  is(frames[0].label, FrameURL, "Got top level document in the list");
  is(frames[1].label, URL, "Got iframe document in the list");

  // Listen to will-navigate to check if the view is empty
  const willNavigate = toolbox.target.once("will-navigate").then(() => {
    info("Navigation to the iframe has started, the inspector should be empty");
    assertMarkupViewIsEmpty(inspector);
  });

  // Only select the iframe after we are able to select an element from the top
  // level document.
  const newRoot = inspector.once("new-root");
  await selectNode("#top", inspector);
  info("Select the iframe");
  frames[0].click();

  await willNavigate;
  await newRoot;

  info("Navigation to the iframe is done, the inspector should be back up");

  // Verify we are on page one
  ok(!(await testActor.hasNode("iframe")),
    "We not longer have access to the top frame elements");
  ok((await testActor.hasNode("#frame")),
    "But now have direct access to the iframe elements");

  // On page 2 load, verify we have the right content
  assertMarkupViewIsLoaded(inspector);

  await selectNode("#frame", inspector);

  Services.prefs.clearUserPref("devtools.command-button-frames.enabled");
});

function assertMarkupViewIsLoaded(inspector) {
  const markupViewBox = inspector.panelDoc.getElementById("markup-box");
  is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
}

function assertMarkupViewIsEmpty(inspector) {
  const markupViewBox = inspector.panelDoc.getElementById("markup-box");
  is(markupViewBox.childNodes.length, 0, "The markup-view is unloaded");
}
