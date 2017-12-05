/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the inspector loads early without waiting for load events.

const server = createTestHTTPServer();

// Register a slow image handler so we can simulate a long time between
// a reload and the load event firing.
server.registerContentType("gif", "image/gif");
function onPageResourceRequest() {
  return new Promise(done => {
    server.registerPathHandler("/slow.gif", function (metadata, response) {
      info("Image has been requested");
      response.processAsync();
      done(response);
    });
  });
}

// Test page load events.
const TEST_URL = "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
  "<p>Page loading slowly</p>" +
  "<img src='http://localhost:" + server.identity.primaryPort + "/slow.gif' />" +
  "</body>" +
  "</html>";

add_task(function* () {
  let {inspector, tab} = yield openInspectorForURL("about:blank");

  let domContentLoaded = waitForLinkedBrowserEvent(tab, "DOMContentLoaded");
  let pageLoaded = waitForLinkedBrowserEvent(tab, "load");

  let markupLoaded = inspector.once("markuploaded");
  let onRequest = onPageResourceRequest();

  info("Navigate to the slow loading page");
  let activeTab = inspector.toolbox.target.activeTab;
  yield activeTab.navigateTo(TEST_URL);

  info("Wait for request made to the image");
  let response = yield onRequest;

  // The request made to the image shouldn't block the DOMContentLoaded event
  info("Wait for DOMContentLoaded");
  yield domContentLoaded;

  // Nor does it prevent the inspector from loading
  info("Wait for markup-loaded");
  yield markupLoaded;

  ok(inspector.markup, "There is a markup view");
  is(inspector.markup._elt.children.length, 1, "The markup view is rendering");
  is(yield contentReadyState(tab), "interactive",
     "Page is still loading but the inspector is ready");

  // Ends page load by unblocking the image request
  response.finish();

  // We should then receive the page load event
  info("Wait for load");
  yield pageLoaded;
});

function waitForLinkedBrowserEvent(tab, event) {
  return BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, event, true);
}

function contentReadyState(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, function () {
    return content.document.readyState;
  });
}
