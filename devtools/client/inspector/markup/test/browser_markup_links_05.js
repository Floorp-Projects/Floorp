/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu items shown when clicking on links in
// attributes actually do the right things.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Select a node with a URI attribute");
  await selectNode("video", inspector);

  info("Set the popupNode to the node that contains the uri");
  let {editor} = await getContainerForSelector("video", inspector);
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("poster").querySelector(".link"),
  });

  info("Follow the link and wait for the new tab to open");
  const onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  inspector.onFollowLink();
  const {target: tab} = await onTabOpened;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  ok(true, "A new tab opened");
  is(tab.linkedBrowser.currentURI.spec, URL_ROOT + "doc_markup_tooltip.png",
    "The URL for the new tab is correct");
  gBrowser.removeTab(tab);

  info("Select a node with a IDREF attribute");
  await selectNode("label", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor} = await getContainerForSelector("label", inspector));
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("for").querySelector(".link"),
  });

  info("Follow the link and wait for the new node to be selected");
  const onSelection = inspector.selection.once("new-node-front");
  inspector.onFollowLink();
  await onSelection;

  ok(true, "A new node was selected");
  is(inspector.selection.nodeFront.id, "name", "The right node was selected");

  info("Select a node with an invalid IDREF attribute");
  await selectNode("output", inspector);

  info("Set the popupNode to the node that contains the ref");
  ({editor} = await getContainerForSelector("output", inspector));
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("for").querySelectorAll(".link")[2],
  });

  info("Try to follow the link and check that no new node were selected");
  const onFailed = inspector.once("idref-attribute-link-failed");
  inspector.onFollowLink();
  await onFailed;

  ok(true, "The node selection failed");
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "output",
    "The <output> node is still selected");
});
