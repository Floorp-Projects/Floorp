/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a middle-click or meta/ctrl-click on links in attributes actually
// do follows the link.

const TEST_URL = URL_ROOT_SSL + "doc_markup_links.html";

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Select a node with a URI attribute");
  await selectNode("video", inspector);

  info("Find the link element from the markup-view");
  let { editor } = await getContainerForSelector("video", inspector);
  let linkEl = editor.attrElements.get("poster").querySelector(".link");

  info("Follow the link with middle-click and wait for the new tab to open");
  await followLinkWaitForTab(
    linkEl,
    false,
    URL_ROOT_SSL + "doc_markup_tooltip.png"
  );

  info("Follow the link with meta/ctrl-click and wait for the new tab to open");
  await followLinkWaitForTab(
    linkEl,
    true,
    URL_ROOT_SSL + "doc_markup_tooltip.png"
  );

  info("Check that simple click does not open a tab");
  const onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  const onTimeout = wait(1000).then(() => "TIMEOUT");
  EventUtils.synthesizeMouseAtCenter(linkEl, {}, linkEl.ownerGlobal);
  const res = await Promise.race([onTabOpened, onTimeout]);
  is(res, "TIMEOUT", "Tab was not opened on simple click");

  info("Select a node with a IDREF attribute");
  await selectNode("label", inspector);

  info("Find the link element from the markup-view that contains the ref");
  ({ editor } = await getContainerForSelector("label", inspector));
  linkEl = editor.attrElements.get("for").querySelector(".link");

  info("Follow link with middle-click, wait for new node to be selected.");
  await followLinkWaitForNewNode(linkEl, false, inspector);

  // We have to re-select the label as the link switched the currently selected
  // node.
  await selectNode("label", inspector);

  info("Follow link with ctrl/meta-click, wait for new node to be selected.");
  await followLinkWaitForNewNode(linkEl, true, inspector);

  info("Select a node with an invalid IDREF attribute");
  await selectNode("output", inspector);

  info("Find the link element from the markup-view that contains the ref");
  ({ editor } = await getContainerForSelector("output", inspector));
  linkEl = editor.attrElements.get("for").querySelectorAll(".link")[2];

  info("Try to follow link wiith middle-click, check no new node selected");
  await followLinkNoNewNode(linkEl, false, inspector);

  info("Try to follow link wiith meta/ctrl-click, check no new node selected");
  await followLinkNoNewNode(linkEl, true, inspector);
});

function performMouseDown(linkEl, metactrl) {
  const evt = linkEl.ownerDocument.createEvent("MouseEvents");

  let button = -1;

  if (metactrl) {
    info("Performing Meta/Ctrl+Left Click");
    button = 0;
  } else {
    info("Performing Middle Click");
    button = 1;
  }

  evt.initMouseEvent(
    "mousedown",
    true,
    true,
    linkEl.ownerDocument.defaultView,
    1,
    0,
    0,
    0,
    0,
    metactrl,
    false,
    false,
    metactrl,
    button,
    null
  );

  linkEl.dispatchEvent(evt);
}

async function followLinkWaitForTab(linkEl, isMetaClick, expectedTabURI) {
  const onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  performMouseDown(linkEl, isMetaClick);
  const { target } = await onTabOpened;
  await BrowserTestUtils.browserLoaded(target.linkedBrowser);
  ok(true, "A new tab opened");
  is(
    target.linkedBrowser.currentURI.spec,
    expectedTabURI,
    "The URL for the new tab is correct"
  );
  gBrowser.removeTab(target);
}

async function followLinkWaitForNewNode(linkEl, isMetaClick, inspector) {
  const onSelection = inspector.selection.once("new-node-front");
  performMouseDown(linkEl, isMetaClick);
  await onSelection;

  ok(true, "A new node was selected");
  is(inspector.selection.nodeFront.id, "name", "The right node was selected");
}

async function followLinkNoNewNode(linkEl, isMetaClick, inspector) {
  const onFailed = inspector.markup.once("idref-attribute-link-failed");
  performMouseDown(linkEl, isMetaClick);
  await onFailed;

  ok(true, "The node selection failed");
  is(
    inspector.selection.nodeFront.tagName.toLowerCase(),
    "output",
    "The <output> node is still selected"
  );
}
