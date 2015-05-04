/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu items shown when clicking on links in
// attributes actually do the right things.

const TEST_URL = TEST_URL_ROOT + "doc_markup_links.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  let linkFollow = inspector.panelDoc.getElementById("node-menu-link-follow");
  let linkCopy = inspector.panelDoc.getElementById("node-menu-link-copy");

  info("Select a node with a URI attribute");
  yield selectNode("video", inspector);

  info("Set the popupNode to the node that contains the uri");
  let {editor} = yield getContainerForSelector("video", inspector);
  let popupNode = editor.attrElements.get("poster").querySelector(".link");
  inspector.panelDoc.popupNode = popupNode;

  info("Follow the link and wait for the new tab to open");
  let onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  inspector.followAttributeLink();
  let {target: tab} = yield onTabOpened;
  yield waitForTabLoad(tab);

  ok(true, "A new tab opened");
  is(tab.linkedBrowser.currentURI.spec, TEST_URL_ROOT + "doc_markup_tooltip.png",
    "The URL for the new tab is correct");
  gBrowser.removeTab(tab);

  info("Select a node with a IDREF attribute");
  yield selectNode("label", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor}) = yield getContainerForSelector("label", inspector);
  popupNode = editor.attrElements.get("for").querySelector(".link");
  inspector.panelDoc.popupNode = popupNode;

  info("Follow the link and wait for the new node to be selected");
  let onSelection = inspector.selection.once("new-node-front");
  inspector.followAttributeLink();
  yield onSelection;

  ok(true, "A new node was selected");
  is(inspector.selection.nodeFront.id, "name", "The right node was selected");

  info("Select a node with an invalid IDREF attribute");
  yield selectNode("output", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor}) = yield getContainerForSelector("output", inspector);
  popupNode = editor.attrElements.get("for").querySelectorAll(".link")[2];
  inspector.panelDoc.popupNode = popupNode;

  info("Try to follow the link and check that no new node were selected");
  let onFailed = inspector.once("idref-attribute-link-failed");
  inspector.followAttributeLink();
  yield onFailed;

  ok(true, "The node selection failed");
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "output",
    "The <output> node is still selected");
});

function waitForTabLoad(tab) {
  let def = promise.defer();
  tab.addEventListener("load", function onLoad(e) {
    // Skip load event for about:blank
    if (tab.linkedBrowser.currentURI.spec === "about:blank") {
      return;
    }
    tab.removeEventListener("load", onLoad);
    def.resolve();
  });
  return def.promise;
}
