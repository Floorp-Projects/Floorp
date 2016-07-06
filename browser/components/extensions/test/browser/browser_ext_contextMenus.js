/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: function() {
      browser.contextMenus.create({
        id: "clickme",
        title: "Click me!",
        contexts: ["image"],
      });
      browser.test.notifyPass();
    },
  });

  yield extension.startup();
  yield extension.awaitFinish();

  let contentAreaContextMenu = yield openContextMenu("#img1");
  let item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 1, "contextMenu item for image was found");
  yield closeContextMenu();

  contentAreaContextMenu = yield openContextMenu("body");
  item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 0, "no contextMenu item for image was found");
  yield closeContextMenu();

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
});

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
      function genericOnClick(info, tab) {
        browser.test.sendMessage("onclick", {info, tab});
      }

      browser.contextMenus.onClicked.addListener((info, tab) => {
        browser.test.sendMessage("browser.contextMenus.onClicked", {info, tab});
      });

      browser.contextMenus.create({
        contexts: ["all"],
        type: "separator",
      });

      let contexts = ["page", "selection", "image"];
      for (let i = 0; i < contexts.length; i++) {
        let context = contexts[i];
        let title = context;
        browser.contextMenus.create({
          title: title,
          contexts: [context],
          id: "ext-" + context,
          onclick: genericOnClick,
        });
        if (context == "selection") {
          browser.contextMenus.update("ext-selection", {
            title: "selection is: '%s'",
            onclick: (info, tab) => {
              browser.contextMenus.removeAll();
              genericOnClick(info, tab);
            },
          });
        }
      }

      let parent = browser.contextMenus.create({
        title: "parent",
      });
      browser.contextMenus.create({
        title: "child1",
        parentId: parent,
        onclick: genericOnClick,
      });
      let child2 = browser.contextMenus.create({
        title: "child2",
        parentId: parent,
        onclick: genericOnClick,
      });

      let parentToDel = browser.contextMenus.create({
        title: "parentToDel",
      });
      browser.contextMenus.create({
        title: "child1",
        parentId: parentToDel,
        onclick: genericOnClick,
      });
      browser.contextMenus.create({
        title: "child2",
        parentId: parentToDel,
        onclick: genericOnClick,
      });
      browser.contextMenus.remove(parentToDel);

      browser.contextMenus.create({
        title: "Without onclick property",
        id: "ext-without-onclick",
      });

      browser.contextMenus.update(parent, {parentId: child2}).then(
        () => {
          browser.test.notifyFail("contextmenus");
        },
        () => {
          browser.test.notifyPass("contextmenus");
        }
      );
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("contextmenus");

  let expectedClickInfo = {
    menuItemId: "ext-image",
    mediaType: "image",
    srcUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/ctxmenu-image.png",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
  };

  function checkClickInfo(result) {
    for (let i of Object.keys(expectedClickInfo)) {
      is(result.info[i], expectedClickInfo[i],
         "click info " + i + " expected to be: " + expectedClickInfo[i] + " but was: " + info[i]);
    }
    is(expectedClickInfo.pageSrc, result.tab.url);
  }

  let extensionMenuRoot = yield openExtensionContextMenu();

  // Check some menu items
  let items = extensionMenuRoot.getElementsByAttribute("label", "image");
  is(items.length, 1, "contextMenu item for image was found (context=image)");
  let image = items[0];

  items = extensionMenuRoot.getElementsByAttribute("label", "selection-edited");
  is(items.length, 0, "contextMenu item for selection was not found (context=image)");

  items = extensionMenuRoot.getElementsByAttribute("label", "parentToDel");
  is(items.length, 0, "contextMenu item for removed parent was not found (context=image)");

  items = extensionMenuRoot.getElementsByAttribute("label", "parent");
  is(items.length, 1, "contextMenu item for parent was found (context=image)");

  is(items[0].childNodes[0].childNodes.length, 2, "child items for parent were found (context=image)");

  // Click on ext-image item and check the click results
  yield closeExtensionContextMenu(image);

  let result = yield extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = yield extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  // Select some text
  yield ContentTask.spawn(gBrowser.selectedBrowser, { }, function* (arg) {
    let doc = content.document;
    let range = doc.createRange();
    let selection = content.getSelection();
    selection.removeAllRanges();
    let textNode = doc.getElementById("img1").previousSibling;
    range.setStart(textNode, 0);
    range.setEnd(textNode, 100);
    selection.addRange(range);
  });

  // Bring up context menu again
  extensionMenuRoot = yield openExtensionContextMenu();

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "Without onclick property");
  is(items.length, 1, "contextMenu item was found (context=page)");

  yield closeExtensionContextMenu(items[0]);

  expectedClickInfo = {
    menuItemId: "ext-without-onclick",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
  };

  result = yield extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  // Bring up context menu again
  extensionMenuRoot = yield openExtensionContextMenu();

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "selection is: 'just some text 123456789012345678901234567890...'");
  is(items.length, 1, "contextMenu item for selection was found (context=selection)");
  let selectionItem = items[0];

  items = extensionMenuRoot.getElementsByAttribute("label", "selection");
  is(items.length, 0, "contextMenu item label update worked (context=selection)");

  yield closeExtensionContextMenu(selectionItem);

  expectedClickInfo = {
    menuItemId: "ext-selection",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
    selectionText: "just some text 1234567890123456789012345678901234567890123456789012345678901234567890123456789012",
  };

  result = yield extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = yield extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  let contentAreaContextMenu = yield openContextMenu("#img1");
  items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(items.length, 0, "top level item was not found (after removeAll()");
  yield closeContextMenu();

  yield extension.unload();
  yield BrowserTestUtils.removeTab(tab1);
});
