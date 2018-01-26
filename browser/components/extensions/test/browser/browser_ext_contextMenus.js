/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: function() {
      browser.contextMenus.create({
        id: "clickme-image",
        title: "Click me!",
        contexts: ["image"],
      });
      browser.contextMenus.create({
        id: "clickme-page",
        title: "Click me!",
        contexts: ["page"],
      });
      browser.contextMenus.onClicked.addListener(() => {});
      browser.test.notifyPass();
    },
  });

  await extension.startup();
  await extension.awaitFinish();

  let contentAreaContextMenu = await openContextMenu("#img1");
  let item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 1, "contextMenu item for image was found");
  await closeContextMenu();

  contentAreaContextMenu = await openContextMenu("body");
  item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 1, "contextMenu item for page was found");
  await closeContextMenu();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: async function() {
      browser.test.onMessage.addListener(msg => {
        if (msg == "removeall") {
          browser.contextMenus.removeAll();
          browser.test.sendMessage("removed");
        }
      });

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

      let contexts = ["page", "link", "selection", "image", "editable", "password"];
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
            onclick: genericOnClick,
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

      await browser.test.assertRejects(
        browser.contextMenus.update(parent, {parentId: child2}),
        /cannot be an ancestor/,
        "Should not be able to reparent an item as descendent of itself");

      browser.test.notifyPass("contextmenus");
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextmenus");

  let expectedClickInfo = {
    menuItemId: "ext-image",
    mediaType: "image",
    srcUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/ctxmenu-image.png",
    pageUrl: PAGE,
    editable: false,
  };

  function checkClickInfo(result) {
    for (let i of Object.keys(expectedClickInfo)) {
      is(result.info[i], expectedClickInfo[i],
         "click info " + i + " expected to be: " + expectedClickInfo[i] + " but was: " + result.info[i]);
    }
    is(expectedClickInfo.pageSrc, result.tab.url, "click info page source is the right tab");
  }

  let extensionMenuRoot = await openExtensionContextMenu();

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
  await closeExtensionContextMenu(image);

  let result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);


  // Test "link" context and OnClick data property.
  extensionMenuRoot = await openExtensionContextMenu("[href=some-link]");

  // Click on ext-link and check the click results
  items = extensionMenuRoot.getElementsByAttribute("label", "link");
  is(items.length, 1, "contextMenu item for parent was found (context=link)");
  let link = items[0];

  expectedClickInfo = {
    menuItemId: "ext-link",
    linkUrl: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/some-link",
    linkText: "Some link",
    pageUrl: PAGE,
    editable: false,
  };

  await closeExtensionContextMenu(link);

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);


  // Test "editable" context and OnClick data property.
  extensionMenuRoot = await openExtensionContextMenu("#edit-me");

  // Check some menu items.
  items = extensionMenuRoot.getElementsByAttribute("label", "editable");
  is(items.length, 1, "contextMenu item for text input element was found (context=editable)");
  let editable = items[0];

  // Click on ext-editable item and check the click results.
  await closeExtensionContextMenu(editable);

  expectedClickInfo = {
    menuItemId: "ext-editable",
    pageUrl: PAGE,
    editable: true,
  };

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  extensionMenuRoot = await openExtensionContextMenu("#password");
  items = extensionMenuRoot.getElementsByAttribute("label", "password");
  is(items.length, 1, "contextMenu item for password input element was found (context=password)");
  let password = items[0];
  await closeExtensionContextMenu(password);
  expectedClickInfo = {
    menuItemId: "ext-password",
    pageUrl: PAGE,
    editable: true,
  };

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  // Select some text
  await ContentTask.spawn(gBrowser.selectedBrowser, { }, async function(arg) {
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
  extensionMenuRoot = await openExtensionContextMenu();

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "Without onclick property");
  is(items.length, 1, "contextMenu item was found (context=page)");

  await closeExtensionContextMenu(items[0]);

  expectedClickInfo = {
    menuItemId: "ext-without-onclick",
    pageUrl: PAGE,
  };

  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  // Bring up context menu again
  extensionMenuRoot = await openExtensionContextMenu();

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "selection is: 'just some text 12345678901234567890123456789012\u2026'");
  is(items.length, 1, "contextMenu item for selection was found (context=selection)");
  let selectionItem = items[0];

  items = extensionMenuRoot.getElementsByAttribute("label", "selection");
  is(items.length, 0, "contextMenu item label update worked (context=selection)");

  await closeExtensionContextMenu(selectionItem);

  expectedClickInfo = {
    menuItemId: "ext-selection",
    pageUrl: PAGE,
    selectionText: " just some text 1234567890123456789012345678901234567890123456789012345678901234567890123456789012",
  };

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);

  // Select a lot of text
  await ContentTask.spawn(gBrowser.selectedBrowser, { }, function* (arg) {
    let doc = content.document;
    let range = doc.createRange();
    let selection = content.getSelection();
    selection.removeAllRanges();
    let textNode = doc.getElementById("longtext").firstChild;
    range.setStart(textNode, 0);
    range.setEnd(textNode, textNode.length);
    selection.addRange(range);
  });

  // Bring up context menu again
  extensionMenuRoot = await openExtensionContextMenu("#longtext");

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "selection is: 'Sed ut perspiciatis unde omnis iste natus error\u2026'");
  is(items.length, 1, `contextMenu item for longtext selection was found (context=selection)`);
  await closeExtensionContextMenu(items[0]);

  expectedClickInfo = {
    menuItemId: "ext-selection",
    pageUrl: PAGE,
  };

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);
  ok(result.info.selectionText.endsWith("quo voluptas nulla pariatur?"), "long text selection worked");


  // Select a lot of text, excercise the nsIDOMNSEditableElement code path in
  // the Browser:GetSelection handler.
  await ContentTask.spawn(gBrowser.selectedBrowser, { }, function(arg) {
    let doc = content.document;
    let node = doc.getElementById("editabletext");
    // content.js handleContentContextMenu fails intermittently without focus.
    node.focus();
    node.selectionStart = 0;
    node.selectionEnd = 844;
  });

  // Bring up context menu again
  extensionMenuRoot = await openExtensionContextMenu("#editabletext");

  // Check some menu items
  items = extensionMenuRoot.getElementsByAttribute("label", "editable");
  is(items.length, 1, "contextMenu item for text input element was found (context=editable)");
  await closeExtensionContextMenu(items[0]);

  expectedClickInfo = {
    menuItemId: "ext-editable",
    editable: true,
    pageUrl: PAGE,
  };

  result = await extension.awaitMessage("onclick");
  checkClickInfo(result);
  result = await extension.awaitMessage("browser.contextMenus.onClicked");
  checkClickInfo(result);
  ok(result.info.selectionText.endsWith("perferendis doloribus asperiores repellat."), "long text selection worked");

  extension.sendMessage("removeall");
  await extension.awaitMessage("removed");

  let contentAreaContextMenu = await openContextMenu("#img1");
  items = contentAreaContextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(items.length, 0, "top level item was not found (after removeAll()");
  await closeContextMenu();

  await extension.unload();
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function testRemoveAllWithTwoExtensions() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  const manifest = {permissions: ["contextMenus"]};

  const first = ExtensionTestUtils.loadExtension({manifest, background() {
    browser.contextMenus.create({title: "alpha", contexts: ["all"]});

    browser.contextMenus.onClicked.addListener(() => {
      browser.contextMenus.removeAll();
    });
    browser.test.onMessage.addListener(msg => {
      if (msg == "ping") {
        browser.test.sendMessage("pong-alpha");
        return;
      }
      browser.contextMenus.create({title: "gamma", contexts: ["all"]});
    });
  }});

  const second = ExtensionTestUtils.loadExtension({manifest, background() {
    browser.contextMenus.create({title: "beta", contexts: ["all"]});

    browser.contextMenus.onClicked.addListener(() => {
      browser.contextMenus.removeAll();
    });

    browser.test.onMessage.addListener(() => {
      browser.test.sendMessage("pong-beta");
    });
  }});

  await first.startup();
  await second.startup();

  async function confirmMenuItems(...items) {
    // Round-trip to extension to make sure that the context menu state has been
    // updated by the async contextMenus.create / contextMenus.removeAll calls.
    first.sendMessage("ping");
    second.sendMessage("ping");
    await first.awaitMessage("pong-alpha");
    await second.awaitMessage("pong-beta");

    const menu = await openContextMenu();
    for (const id of ["alpha", "beta", "gamma"]) {
      const expected = items.includes(id);
      const found = menu.getElementsByAttribute("label", id);
      is(found.length, expected, `menu item ${id} ${expected ? "" : "not "}found`);
    }
    // Return the first menu item, we need to click it.
    return menu.getElementsByAttribute("label", items[0])[0];
  }

  // Confirm alpha, beta exist; click alpha to remove it.
  const alpha = await confirmMenuItems("alpha", "beta");
  await closeExtensionContextMenu(alpha);

  // Confirm only beta exists.
  await confirmMenuItems("beta");
  await closeContextMenu();

  // Create gamma, confirm, click.
  first.sendMessage("create");
  const beta = await confirmMenuItems("beta", "gamma");
  await closeExtensionContextMenu(beta);

  // Confirm only gamma is left.
  await confirmMenuItems("gamma");
  await closeContextMenu();

  await first.unload();
  await second.unload();
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_bookmark_contextmenu() {
  const bookmarksToolbar = document.getElementById("PersonalToolbar");
  setToolbarVisibility(bookmarksToolbar, true);

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus", "bookmarks"],
    },
    async background() {
      const url = "https://example.com/";
      const title = "Example";
      let newBookmark = await browser.bookmarks.create({
        url,
        title,
        parentId: "toolbar_____",
      });
      await browser.contextMenus.create({
        title: "Get bookmark",
        contexts: ["bookmark"],
      });
      browser.test.sendMessage("bookmark-created");
      browser.contextMenus.onClicked.addListener(async (info) => {
        browser.test.assertEq(newBookmark.id, info.bookmarkId, "Bookmark ID matches");

        let [bookmark] = await browser.bookmarks.get(info.bookmarkId);
        browser.test.assertEq(title, bookmark.title, "Bookmark title matches");
        browser.test.assertEq(url, bookmark.url, "Bookmark url matches");
        browser.test.assertFalse(info.hasOwnProperty("pageUrl"), "Context menu does not expose pageUrl");
        await browser.bookmarks.remove(info.bookmarkId);
        browser.test.sendMessage("test-finish");
      });
    },
  });
  await extension.startup();
  await extension.awaitMessage("bookmark-created");
  let menu = await openChromeContextMenu(
    "placesContext",
    "#PersonalToolbar .bookmark-item:last-child");

  let menuItem = menu.getElementsByAttribute("label", "Get bookmark")[0];
  closeChromeContextMenu("placesContext", menuItem);

  await extension.awaitMessage("test-finish");
  await extension.unload();
  setToolbarVisibility(bookmarksToolbar, false);
});

add_task(async function test_bookmark_context_requires_permission() {
  const bookmarksToolbar = document.getElementById("PersonalToolbar");
  setToolbarVisibility(bookmarksToolbar, true);

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    async background() {
      await browser.contextMenus.create({
        title: "Get bookmark",
        contexts: ["bookmark"],
      });
      browser.test.sendMessage("bookmark-created");
    },
  });
  await extension.startup();
  await extension.awaitMessage("bookmark-created");
  let menu = await openChromeContextMenu(
    "placesContext",
    "#PersonalToolbar .bookmark-item:last-child");

  Assert.equal(menu.getElementsByAttribute("label", "Get bookmark").length, 0,
               "bookmark context menu not created with `bookmarks` permission.");

  closeChromeContextMenu("placesContext");

  await extension.unload();
  setToolbarVisibility(bookmarksToolbar, false);
});
