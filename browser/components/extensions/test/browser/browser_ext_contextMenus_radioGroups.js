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
        title: "radio-group-1",
        type: "radio",
        checked: true,
      });

      browser.contextMenus.create({
        type: "separator",
      });

      browser.contextMenus.create({
        title: "radio-group-2",
        type: "radio",
      });

      browser.contextMenus.create({
        title: "radio-group-2",
        type: "radio",
      });

      browser.test.notifyPass("contextmenus-radio-groups");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("contextmenus-radio-groups");

  function confirmRadioGroupStates(extensionMenuRoot, expectedStates) {
    let radioItems = extensionMenuRoot.getElementsByAttribute("type", "radio");
    let radioGroup1 = extensionMenuRoot.getElementsByAttribute("label", "radio-group-1");
    let radioGroup2 = extensionMenuRoot.getElementsByAttribute("label", "radio-group-2");

    is(radioItems.length, 3, "there should be 3 radio items in the context menu");
    is(radioGroup1.length, 1, "the first radio group should only have 1 radio item");
    is(radioGroup2.length, 2, "the second radio group should only have 2 radio items");

    is(radioGroup1[0].hasAttribute("checked"), expectedStates[0], `radio item 1 has state (checked=${expectedStates[0]})`);
    is(radioGroup2[0].hasAttribute("checked"), expectedStates[1], `radio item 2 has state (checked=${expectedStates[1]})`);
    is(radioGroup2[1].hasAttribute("checked"), expectedStates[2], `radio item 3 has state (checked=${expectedStates[2]})`);

    return extensionMenuRoot.getElementsByAttribute("type", "radio");
  }

  let extensionMenuRoot = yield openExtensionContextMenu();
  let items = confirmRadioGroupStates(extensionMenuRoot, [true, false, false]);
  yield closeExtensionContextMenu(items[1]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmRadioGroupStates(extensionMenuRoot, [true, true, false]);
  yield closeExtensionContextMenu(items[2]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmRadioGroupStates(extensionMenuRoot, [true, false, true]);
  yield closeExtensionContextMenu(items[0]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmRadioGroupStates(extensionMenuRoot, [true, false, true]);
  yield closeExtensionContextMenu(items[0]);

  yield extension.unload();
  yield BrowserTestUtils.removeTab(tab1);
});
