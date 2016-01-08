/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a middle-click or meta/ctrl-click on links in attributes actually
// do follows the link.

const TEST_URL = TEST_URL_ROOT + "doc_markup_links.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Select a node with a URI attribute");
  yield selectNode("video", inspector);

  info("Find the link element from the markup-view");
  let {editor} = yield getContainerForSelector("video", inspector);
  let linkEl = editor.attrElements.get("poster").querySelector(".link");

  info("Follow the link with middle-click and wait for the new tab to open");
  yield followLinkWaitForTab(linkEl, false,
                             TEST_URL_ROOT + "doc_markup_tooltip.png");

  info("Follow the link with meta/ctrl-click and wait for the new tab to open");
  yield followLinkWaitForTab(linkEl, true,
                             TEST_URL_ROOT + "doc_markup_tooltip.png");

  info("Select a node with a IDREF attribute");
  yield selectNode("label", inspector);

  info("Find the link element from the markup-view that contains the ref");
  ({editor} = yield getContainerForSelector("label", inspector));
  linkEl = editor.attrElements.get("for").querySelector(".link");

  info("Follow link with middle-click, wait for new node to be selected.");
  yield followLinkWaitForNewNode(linkEl, false, inspector);

  // We have to re-select the label as the link switched the currently selected node
  yield selectNode("label", inspector);

  info("Follow link with ctrl/meta-click, wait for new node to be selected.");
  yield followLinkWaitForNewNode(linkEl, true, inspector);

  info("Select a node with an invalid IDREF attribute");
  yield selectNode("output", inspector);

  info("Find the link element from the markup-view that contains the ref");
  ({editor} = yield getContainerForSelector("output", inspector));
  linkEl = editor.attrElements.get("for").querySelectorAll(".link")[2];

  info("Try to follow link wiith middle-click, check no new node selected");
  yield followLinkNoNewNode(linkEl, false, inspector);

  info("Try to follow link wiith meta/ctrl-click, check no new node selected");
  yield followLinkNoNewNode(linkEl, true, inspector);
});

function waitForTabLoad(tab) {
  let def = promise.defer();
  tab.addEventListener("load", function onLoad() {
    // Skip load event for about:blank
    if (tab.linkedBrowser.currentURI.spec === "about:blank") {
      return;
    }
    tab.removeEventListener("load", onLoad);
    def.resolve();
  });
  return def.promise;
}

function performMouseDown(linkEl, metactrl) {
  let evt = linkEl.ownerDocument.createEvent("MouseEvents");

  let button = -1;

  if (metactrl) {
    info("Performing Meta/Ctrl+Left Click");
    button = 0;
  } else {
    info("Performing Middle Click");
    button = 1;
  }

  evt.initMouseEvent("mousedown", true, true,
      linkEl.ownerDocument.defaultView, 1, 0, 0, 0, 0, metactrl,
      false, false, metactrl, button, null);

  linkEl.dispatchEvent(evt);
}

function* followLinkWaitForTab(linkEl, isMetaClick, expectedTabURI) {
  let onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  performMouseDown(linkEl, isMetaClick);
  let {target} = yield onTabOpened;
  yield waitForTabLoad(target);
  ok(true, "A new tab opened");
  is(target.linkedBrowser.currentURI.spec, expectedTabURI,
     "The URL for the new tab is correct");
  gBrowser.removeTab(target);
}

function* followLinkWaitForNewNode(linkEl, isMetaClick, inspector) {
  let onSelection = inspector.selection.once("new-node-front");
  performMouseDown(linkEl, isMetaClick);
  yield onSelection;

  ok(true, "A new node was selected");
  is(inspector.selection.nodeFront.id, "name", "The right node was selected");
}

function* followLinkNoNewNode(linkEl, isMetaClick, inspector) {
  let onFailed = inspector.once("idref-attribute-link-failed");
  performMouseDown(linkEl, isMetaClick);
  yield onFailed;

  ok(true, "The node selection failed");
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "output",
    "The <output> node is still selected");
}
