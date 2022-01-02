/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test frame selection switching at toolbox level when using the inspector

const FrameURL =
  "data:text/html;charset=UTF-8," +
  encodeURI('<div id="in-iframe">frame</div>');
const URL =
  "data:text/html;charset=UTF-8," +
  encodeURI('<iframe src="' + FrameURL + '"></iframe><div id="top">top</div>');

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(URL);

  // Verify we are on the top level document
  await assertMarkupViewAsTree(
    `
    body
      iframe!ignore-children
      div id="top"`,
    "body",
    inspector
  );

  assertMarkupViewIsLoaded(inspector);

  // Verify that the frame map button is empty at the moment.
  const btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.firstChild, "The frame list button doesn't have any children");

  // Open frame menu and wait till it's available on the screen.
  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  btn.click();
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  // Verify that the menu is populated.
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));
  is(frames.length, 2, "We have both frames in the menu");

  frames.sort(function(a, b) {
    return a.children[0].innerHTML.localeCompare(b.children[0].innerHTML);
  });

  is(
    frames[0].querySelector(".label").textContent,
    FrameURL,
    "Got top level document in the list"
  );
  is(
    frames[1].querySelector(".label").textContent,
    URL,
    "Got iframe document in the list"
  );

  // Listen to will-navigate to check if the view is empty
  const { resourceCommand } = toolbox.commands;
  const {
    onResource: willNavigate,
  } = await resourceCommand.waitForNextResource(
    resourceCommand.TYPES.DOCUMENT_EVENT,
    {
      ignoreExistingResources: true,
      predicate(resource) {
        return resource.name == "will-navigate";
      },
    }
  );
  willNavigate.then(() => {
    info("Navigation to the iframe has started, the inspector should be empty");
    assertMarkupViewIsEmpty(inspector);
  });

  // Only select the iframe after we are able to select an element from the top
  // level document.
  let newRoot = inspector.once("new-root");
  await selectNode("#top", inspector);
  info("Select the iframe");
  frames[0].click();

  if (!isEveryFrameTargetEnabled()) {
    await willNavigate;
  }
  await newRoot;

  info("The iframe is selected, check that the markup view was updated");
  await assertMarkupViewAsTree(
    `
    body
      div id="in-iframe"`,
    "body",
    inspector
  );
  assertMarkupViewIsLoaded(inspector);

  info(
    "Remove the iframe and check that the inspector gets updated to show the top level frame markup"
  );
  newRoot = inspector.once("new-root");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.document.querySelector("iframe").remove();
  });
  await newRoot;

  await assertMarkupViewAsTree(
    `
    body
      div id="top"`,
    "body",
    inspector
  );
  assertMarkupViewIsLoaded(inspector);
});

function assertMarkupViewIsLoaded(inspector) {
  const markupViewBox = inspector.panelDoc.getElementById("markup-box");
  is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
}

function assertMarkupViewIsEmpty(inspector) {
  const markupFrame = inspector._markupFrame;
  is(
    markupFrame.contentDocument.getElementById("root").childNodes.length,
    0,
    "The markup-view is unloaded"
  );
}
