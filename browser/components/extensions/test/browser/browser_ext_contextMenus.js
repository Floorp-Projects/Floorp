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
        browser.test.sendMessage("menuItemClick", info);
      }

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
            onclick: (info) => {
              browser.contextMenus.removeAll();
              genericOnClick(info);
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
        title: "radio-group-1",
        type: "radio",
        checked: true,
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.create({
        title: "Checkbox",
        type: "checkbox",
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.create({
        title: "radio-group-2",
        type: "radio",
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.create({
        title: "radio-group-2",
        type: "radio",
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.create({
        type: "separator",
      });

      browser.contextMenus.create({
        title: "Checkbox",
        type: "checkbox",
        checked: true,
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.create({
        title: "Checkbox",
        type: "checkbox",
        contexts: ["page"],
        onclick: genericOnClick,
      });

      browser.contextMenus.update(parent, {parentId: child2}).then(
        () => {
          browser.test.notifyFail();
        },
        () => {
          browser.test.notifyPass();
        }
      );
    },
  });

  yield extension.startup();
  yield extension.awaitFinish();

  let contentAreaContextMenu;

  function getTop() {
    contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
    let items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
    is(items.length, 1, "top level item was found (context=selection)");
    let topItem = items[0];
    return topItem.childNodes[0];
  }

  function* openExtensionMenu() {
    contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
    let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
    yield BrowserTestUtils.synthesizeMouseAtCenter("#img1", {
      type: "contextmenu",
      button: 2,
    }, gBrowser.selectedBrowser);
    yield popupShownPromise;

    popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(getTop(), {});
    yield popupShownPromise;
  }

  function* closeContextMenu(itemToSelect, expectedClickInfo) {
    function checkClickInfo(info) {
      for (let i of Object.keys(expectedClickInfo)) {
        is(info[i], expectedClickInfo[i],
           "click info " + i + " expected to be: " + expectedClickInfo[i] + " but was: " + info[i]);
      }
      is(expectedClickInfo.pageSrc, info.tab.url);
    }
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    EventUtils.synthesizeMouseAtCenter(itemToSelect, {});
    let clickInfo = yield extension.awaitMessage("menuItemClick");
    if (expectedClickInfo) {
      checkClickInfo(clickInfo);
    }
    yield popupHiddenPromise;
  }

  function confirmRadioGroupStates(expectedStates) {
    let top = getTop();

    let radioItems = top.getElementsByAttribute("type", "radio");
    let radioGroup1 = top.getElementsByAttribute("label", "radio-group-1");
    let radioGroup2 = top.getElementsByAttribute("label", "radio-group-2");

    is(radioItems.length, 3, "there should be 3 radio items in the context menu");
    is(radioGroup1.length, 1, "the first radio group should only have 1 radio item");
    is(radioGroup2.length, 2, "the second radio group should only have 2 radio items");

    is(radioGroup1[0].hasAttribute("checked"), expectedStates[0], `radio item 1 has state (checked=${expectedStates[0]})`);
    is(radioGroup2[0].hasAttribute("checked"), expectedStates[1], `radio item 2 has state (checked=${expectedStates[1]})`);
    is(radioGroup2[1].hasAttribute("checked"), expectedStates[2], `radio item 3 has state (checked=${expectedStates[2]})`);
  }

  function confirmCheckboxStates(expectedStates) {
    let checkboxItems = getTop().getElementsByAttribute("type", "checkbox");

    is(checkboxItems.length, 3, "there should be 3 checkbox items in the context menu");

    is(checkboxItems[0].hasAttribute("checked"), expectedStates[0], `checkbox item 1 has state (checked=${expectedStates[0]})`);
    is(checkboxItems[1].hasAttribute("checked"), expectedStates[1], `checkbox item 2 has state (checked=${expectedStates[1]})`);
    is(checkboxItems[2].hasAttribute("checked"), expectedStates[2], `checkbox item 3 has state (checked=${expectedStates[2]})`);
  }

  yield openExtensionMenu();

  // Check some menu items
  let top = getTop();
  let items = top.getElementsByAttribute("label", "image");
  is(items.length, 1, "contextMenu item for image was found (context=image)");
  let image = items[0];

  items = top.getElementsByAttribute("label", "selection-edited");
  is(items.length, 0, "contextMenu item for selection was not found (context=image)");

  items = top.getElementsByAttribute("label", "parentToDel");
  is(items.length, 0, "contextMenu item for removed parent was not found (context=image)");

  items = top.getElementsByAttribute("label", "parent");
  is(items.length, 1, "contextMenu item for parent was found (context=image)");

  is(items[0].childNodes[0].childNodes.length, 2, "child items for parent were found (context=image)");

  // Click on ext-image item and check the click results
  yield closeContextMenu(image, {
    menuItemId: "ext-image",
    mediaType: "image",
    srcUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/ctxmenu-image.png",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
  });

  // Test radio groups
  yield openExtensionMenu();
  confirmRadioGroupStates([true, false, false]);
  items = getTop().getElementsByAttribute("type", "radio");
  yield closeContextMenu(items[1]);

  yield openExtensionMenu();
  confirmRadioGroupStates([true, true, false]);
  items = getTop().getElementsByAttribute("type", "radio");
  yield closeContextMenu(items[2]);

  yield openExtensionMenu();
  confirmRadioGroupStates([true, false, true]);
  items = getTop().getElementsByAttribute("type", "radio");
  yield closeContextMenu(items[0]);

  yield openExtensionMenu();
  confirmRadioGroupStates([true, false, true]);

  // Test checkboxes
  items = getTop().getElementsByAttribute("type", "checkbox");
  confirmCheckboxStates([false, true, false]);
  yield closeContextMenu(items[0]);

  yield openExtensionMenu();
  confirmCheckboxStates([true, true, false]);
  items = getTop().getElementsByAttribute("type", "checkbox");
  yield closeContextMenu(items[2]);

  yield openExtensionMenu();
  confirmCheckboxStates([true, true, true]);
  items = getTop().getElementsByAttribute("type", "checkbox");
  yield closeContextMenu(items[0]);

  yield openExtensionMenu();
  confirmCheckboxStates([false, true, true]);
  items = getTop().getElementsByAttribute("type", "checkbox");
  yield closeContextMenu(items[2]);

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
  yield openExtensionMenu();

  // Check some menu items
  top = getTop();
  items = top.getElementsByAttribute("label", "selection is: 'just some text 123456789012345678901234567890...'");
  is(items.length, 1, "contextMenu item for selection was found (context=selection)");
  let selectionItem = items[0];

  items = top.getElementsByAttribute("label", "selection");
  is(items.length, 0, "contextMenu item label update worked (context=selection)");

  yield closeContextMenu(selectionItem, {
    menuItemId: "ext-selection",
    pageUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html",
    selectionText: "just some text 1234567890123456789012345678901234567890123456789012345678901234567890123456789012",
  });

  items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(items.length, 0, "top level item was not found (after removeAll()");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
});
