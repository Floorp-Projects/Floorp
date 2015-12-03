/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals content */
/* eslint-disable mozilla/no-cpows-in-tests */

add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: function() {
      // A generic onclick callback function.
      function genericOnClick(info) {
        browser.test.sendMessage("menuItemClick", JSON.stringify(info));
      }

      browser.contextMenus.create({ "contexts": ["all"], "type": "separator" });

      let contexts = ["page", "selection", "image"];
      for (let i = 0; i < contexts.length; i++) {
        let context = contexts[i];
        let title = context;
        browser.contextMenus.create({ "title": title, "contexts": [context], "id": "ext-" + context,
                                      "onclick": genericOnClick });
        if (context == "selection") {
          browser.contextMenus.update("ext-selection", { "title": "selection-edited" });
        }
      }

      let parent = browser.contextMenus.create({ "title": "parent" });
      browser.contextMenus.create(
        { "title": "child1", "parentId": parent, "onclick": genericOnClick });
      browser.contextMenus.create(
        { "title": "child2", "parentId": parent, "onclick": genericOnClick });

      let parentToDel = browser.contextMenus.create({ "title": "parentToDel" });
      browser.contextMenus.create(
        { "title": "child1", "parentId": parentToDel, "onclick": genericOnClick });
      browser.contextMenus.create(
        { "title": "child2", "parentId": parentToDel, "onclick": genericOnClick });
      browser.contextMenus.remove(parentToDel);

      browser.test.notifyPass();
    },
  });

  let expectedClickInfo;
  function checkClickInfo(info) {
    info = JSON.parse(info);
    for (let i in expectedClickInfo) {
      is(info[i], expectedClickInfo[i],
         "click info " + i + " expected to be: " + expectedClickInfo[i] + " but was: " + info[i]);
    }
    is(expectedClickInfo.pageSrc, info.tab.url);
  }

  yield extension.startup();
  yield extension.awaitFinish();

  // Bring up context menu
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter("#img1",
    { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
  yield popupShownPromise;

  // Check some menu items
  let items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(items.length, 1, "top level item was found (context=image)");
  let topItem = items.item(0);
  let top = topItem.childNodes[0];

  items = top.getElementsByAttribute("label", "image");
  is(items.length, 1, "contextMenu item for image was found (context=image)");
  let image = items.item(0);

  items = top.getElementsByAttribute("label", "selection-edited");
  is(items.length, 0, "contextMenu item for selection was not found (context=image)");

  items = top.getElementsByAttribute("label", "parentToDel");
  is(items.length, 0, "contextMenu item for removed parent was not found (context=image)");

  items = top.getElementsByAttribute("label", "parent");
  is(items.length, 1, "contextMenu item for parent was found (context=image)");

  is(items.item(0).childNodes[0].childNodes.length, 2, "child items for parent were found (context=image)");

  // Click on ext-image item and check the results
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  expectedClickInfo = {
    menuItemId: "ext-image",
    mediaType: "image",
    srcUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/ctxmenu-image.png",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
  };
  top.openPopup(topItem, "end_before", 0, 0, true, false);
  EventUtils.synthesizeMouseAtCenter(image, {});
  let clickInfo = yield extension.awaitMessage("menuItemClick");
  checkClickInfo(clickInfo);
  yield popupHiddenPromise;

  // Select some text
  yield ContentTask.spawn(gBrowser.selectedBrowser, { }, function* (arg) {
    let doc = content.document;
    let range = doc.createRange();
    let selection = content.getSelection();
    selection.removeAllRanges();
    let textNode = doc.getElementById("img1").previousSibling;
    range.setStart(textNode, 0);
    range.setEnd(textNode, 20);
    selection.addRange(range);
  });

  // Bring up context menu again
  popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouse(null, 1, 1,
    { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
  yield popupShownPromise;

  items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(items.length, 1, "top level item was found (context=selection)");
  top = items.item(0).childNodes[0];

  // Check some menu items
  items = top.getElementsByAttribute("label", "selection-edited");
  is(items.length, 1, "contextMenu item for selection was found (context=selection)");
  let selectionItem = items.item(0);

  items = top.getElementsByAttribute("label", "selection");
  is(items.length, 0, "contextMenu item label update worked (context=selection)");

  popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  expectedClickInfo = {
    menuItemId: "ext-selection",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
    selectionText: "just some text",
  };
  top.openPopup(topItem, "end_before", 0, 0, true, false);
  EventUtils.synthesizeMouseAtCenter(selectionItem, {});
  clickInfo = yield extension.awaitMessage("menuItemClick");
  checkClickInfo(clickInfo);
  yield popupHiddenPromise;

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
});
