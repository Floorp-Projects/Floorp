/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that the inspector doesn't go blank when navigating to a page that
// deletes an iframe while loading.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_remove-iframe-during-load.html";

add_task(function* () {
  let {inspector, toolbox} = yield openInspectorForURL("about:blank");
  yield selectNode("body", inspector);

  // We do not want to wait for the inspector to be fully ready before testing
  // so we load TEST_URL and just wait for the content window to be done loading.
  let done = waitForContentMessage("Test:TestPageProcessingDone");
  content.location = TEST_URL;
  yield done;

  // The content doc contains a script that creates iframes and deletes them
  // immediately after. It does this before the load event, after
  // DOMContentLoaded and after load. This is what used to make the inspector go
  // blank when navigating to that page.
  // At this stage, there should be no iframes in the page anymore.
  ok(!getNode("iframe", {expectNoMatch: true}),
    "Iframes added by the content page should have been removed");

  // Create/remove an extra one now, after the load event.
  info("Creating and removing an iframe.");
  let iframe = content.document.createElement("iframe");
  content.document.body.appendChild(iframe);
  iframe.remove();

  ok(!getNode("iframe", {expectNoMatch: true}),
    "The after-load iframe should have been removed.");

  info("Waiting for markup-view to load.");
  yield inspector.once("markuploaded");

  // Assert that the markup-view is displayed and works
  ok(!getNode("iframe", {expectNoMatch: true}), "Iframe has been removed.");
  is(getNode("#yay").textContent, "load", "Load event fired.");

  yield selectNode("#yay", inspector);
});
