/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu items shown when clicking on links in
// attributes actually do the right things.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Select a node with a URI attribute");
  yield selectNode("video", inspector);

  info("Set the popupNode to the node that contains the uri");
  let {editor} = yield getContainerForSelector("video", inspector);
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("poster").querySelector(".link"),
  });

  info("Follow the link and wait for the new tab to open");
  let onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  inspector.onFollowLink();
  let {target: tab} = yield onTabOpened;
  yield waitForTabLoad(tab);

  ok(true, "A new tab opened");
  is(tab.linkedBrowser.currentURI.spec, URL_ROOT + "doc_markup_tooltip.png",
    "The URL for the new tab is correct");
  gBrowser.removeTab(tab);

  info("Select a node with a IDREF attribute");
  yield selectNode("label", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor} = yield getContainerForSelector("label", inspector));
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("for").querySelector(".link"),
  });

  info("Follow the link and wait for the new node to be selected");
  let onSelection = inspector.selection.once("new-node-front");
  inspector.onFollowLink();
  yield onSelection;

  ok(true, "A new node was selected");
  is(inspector.selection.nodeFront.id, "name", "The right node was selected");

  info("Select a node with an invalid IDREF attribute");
  yield selectNode("output", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor} = yield getContainerForSelector("output", inspector));
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("for").querySelectorAll(".link")[2],
  });

  info("Try to follow the link and check that no new node were selected");
  let onFailed = inspector.once("idref-attribute-link-failed");
  inspector.onFollowLink();
  yield onFailed;

  ok(true, "The node selection failed");
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "output",
    "The <output> node is still selected");
});

function waitForTabLoad(tab) {
  let def = defer();
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
