/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view continues to work after a page navigation and that
// it also works after going back

add_task(function*() {
  yield addTab(URL_ROOT + "doc_layout_iframe1.html");
  let {toolbox, inspector, view} = yield openLayoutView();
  yield runTests(inspector, view);
});

addTest("Test that the layout-view works on the first page",
function*(inspector, view) {
  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value");
  let paddingElt = view.doc.querySelector(".padding.top > span");
  is(paddingElt.textContent, "50");

  info("Listening for layout-view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  getNode("p").style.padding = "20px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(paddingElt.textContent, "20");
});

addTest("Navigate to the second page",
function*(inspector, view) {
  yield navigateTo(URL_ROOT + "doc_layout_iframe2.html");
  yield inspector.once("markuploaded");
});

addTest("Test that the layout-view works on the second page",
function*(inspector, view) {
  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".size > span");
  is(sizeElt.textContent, "100" + "\u00D7" + "100");

  info("Listening for layout-view changes and modifying the size");
  let onUpdated = waitForUpdate(inspector);
  getNode("p").style.width = "200px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200" + "\u00D7" + "100");
});

addTest("Go back to the first page",
function*(inspector, view) {
  content.history.back();
  yield inspector.once("markuploaded");
});

addTest("Test that the layout-view works on the first page after going back",
function*(inspector, view) {
  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value, which is the" +
    "modified value from step one because of the bfcache");
  let paddingElt = view.doc.querySelector(".padding.top > span");
  is(paddingElt.textContent, "20");

  info("Listening for layout-view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  getNode("p").style.padding = "100px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(paddingElt.textContent, "100");
});

function navigateTo(url) {
  info("Navigating to " + url);

  let def = promise.defer();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL " + url + " loading complete");
    waitForFocus(def.resolve, content);
  }, true);
  content.location = url;

  return def.promise;
}
