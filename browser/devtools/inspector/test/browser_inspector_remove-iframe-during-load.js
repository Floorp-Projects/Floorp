/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that the inspector doesn't go blank when navigating to a page that
// deletes an iframe while loading.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_remove-iframe-during-load.html";

let test = asyncTest(function* () {
  let { inspector, toolbox } = yield openInspectorForURL("about:blank");

  yield selectNode("body", inspector);

  let loaded = once(gBrowser.selectedBrowser, "load", true);
  content.location = TEST_URL;

  info("Waiting for test page to load.");
  yield loaded;

  // The content doc contains a script that creates an iframe and deletes
  // it immediately after. This is what used to make the inspector go
  // blank when navigating to that page.
  info("Creating and removing an iframe.");
  var iframe = content.document.createElement("iframe");
  content.document.body.appendChild(iframe);
  iframe.remove();

  ok(!getNode("iframe", {expectNoMatch: true}), "Iframe has been removed.");

  info("Waiting for markup-view to load.");
  yield inspector.once("markuploaded");

  // Assert that the markup-view is displayed and works
  ok(!getNode("iframe", {expectNoMatch: true}), "Iframe has been removed.");
  is(getNode("#yay").textContent, "load", "Load event fired.");

  yield selectNode("#yay", inspector);
});
