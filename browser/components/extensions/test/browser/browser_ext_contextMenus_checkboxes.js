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
        title: "Checkbox",
        type: "checkbox",
      });

      browser.contextMenus.create({
        type: "separator",
      });

      browser.contextMenus.create({
        title: "Checkbox",
        type: "checkbox",
        checked: true,
      });

      browser.contextMenus.create({
        title: "Checkbox",
        type: "checkbox",
      });

      browser.test.notifyPass("contextmenus-checkboxes");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("contextmenus-checkboxes");

  function confirmCheckboxStates(extensionMenuRoot, expectedStates) {
    let checkboxItems = extensionMenuRoot.getElementsByAttribute("type", "checkbox");

    is(checkboxItems.length, 3, "there should be 3 checkbox items in the context menu");

    is(checkboxItems[0].hasAttribute("checked"), expectedStates[0], `checkbox item 1 has state (checked=${expectedStates[0]})`);
    is(checkboxItems[1].hasAttribute("checked"), expectedStates[1], `checkbox item 2 has state (checked=${expectedStates[1]})`);
    is(checkboxItems[2].hasAttribute("checked"), expectedStates[2], `checkbox item 3 has state (checked=${expectedStates[2]})`);

    return extensionMenuRoot.getElementsByAttribute("type", "checkbox");
  }

  let extensionMenuRoot = yield openExtensionContextMenu();
  let items = confirmCheckboxStates(extensionMenuRoot, [false, true, false]);
  yield closeExtensionContextMenu(items[0]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmCheckboxStates(extensionMenuRoot, [true, true, false]);
  yield closeExtensionContextMenu(items[2]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmCheckboxStates(extensionMenuRoot, [true, true, true]);
  yield closeExtensionContextMenu(items[0]);

  extensionMenuRoot = yield openExtensionContextMenu();
  items = confirmCheckboxStates(extensionMenuRoot, [false, true, true]);
  yield closeExtensionContextMenu(items[2]);

  yield extension.unload();
  yield BrowserTestUtils.removeTab(tab1);
});
