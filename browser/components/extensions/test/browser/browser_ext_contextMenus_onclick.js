/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

// Loaded both as a background script and a tab page.
function testScript() {
  let page = location.pathname.includes("tab.html") ? "tab" : "background";
  let clickCounts = {
    old: 0,
    new: 0,
  };
  browser.contextMenus.onClicked.addListener(() => {
    // Async to give other onclick handlers a chance to fire.
    setTimeout(() => {
      browser.test.sendMessage("onClicked-fired", page);
    });
  });
  browser.test.onMessage.addListener((toPage, msg) => {
    if (toPage !== page) {
      return;
    }
    browser.test.log(`Received ${msg} for ${toPage}`);
    if (msg == "get-click-counts") {
      browser.test.sendMessage("click-counts", clickCounts);
    } else if (msg == "clear-click-counts") {
      clickCounts.old = clickCounts.new = 0;
      browser.test.sendMessage("next");
    } else if (msg == "create-with-onclick") {
      browser.contextMenus.create({
        id: "iden",
        title: "tifier",
        onclick() {
          ++clickCounts.old;
          browser.test.log(`onclick fired for original onclick property in ${page}`);
        },
      }, () => browser.test.sendMessage("next"));
    } else if (msg == "create-without-onclick") {
      browser.contextMenus.create({
        id: "iden",
        title: "tifier",
      }, () => browser.test.sendMessage("next"));
    } else if (msg == "update-without-onclick") {
      browser.contextMenus.update("iden", {
        enabled: true,  // Already enabled, so this does nothing.
      }, () => browser.test.sendMessage("next"));
    } else if (msg == "update-with-onclick") {
      browser.contextMenus.update("iden", {
        onclick() {
          ++clickCounts.new;
          browser.test.log(`onclick fired for updated onclick property in ${page}`);
        },
      }, () => browser.test.sendMessage("next"));
    } else if (msg == "remove") {
      browser.contextMenus.remove("iden", () => browser.test.sendMessage("next"));
    } else if (msg == "removeAll") {
      browser.contextMenus.removeAll(() => browser.test.sendMessage("next"));
    }
  });

  if (page == "background") {
    browser.test.log("Opening tab.html");
    browser.tabs.create({
      url: "tab.html",
      active: false,  // To not interfere with the context menu tests.
    });
  } else {
    // Sanity check - the pages must be in the same process.
    let pages = browser.extension.getViews();
    browser.test.assertTrue(pages.includes(window),
                            "Expected this tab to be an extension view");
    pages = pages.filter(w => w !== window);
    browser.test.assertEq(pages[0], browser.extension.getBackgroundPage(),
                          "Expected the other page to be a background page");
    browser.test.sendMessage("tab.html ready");
  }
}

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },
    background: testScript,
    files: {
      "tab.html": `<!DOCTYPE html><meta charset="utf-8"><script src="tab.js"></script>`,
      "tab.js": testScript,
    },
  });
  await extension.startup();
  await extension.awaitMessage("tab.html ready");

  async function clickContextMenu() {
    // Using openContextMenu instead of openExtensionContextMenu because the
    // test extension has only one context menu item.
    let extensionMenuRoot = await openContextMenu();
    let items = extensionMenuRoot.getElementsByAttribute("label", "tifier");
    is(items.length, 1, "Expected one context menu item");
    await closeExtensionContextMenu(items[0]);
    // One of them is "tab", the other is "background".
    info(`onClicked from: ${await extension.awaitMessage("onClicked-fired")}`);
    info(`onClicked from: ${await extension.awaitMessage("onClicked-fired")}`);
  }

  function getCounts(page) {
    extension.sendMessage(page, "get-click-counts");
    return extension.awaitMessage("click-counts");
  }
  async function resetCounts() {
    extension.sendMessage("tab", "clear-click-counts");
    extension.sendMessage("background", "clear-click-counts");
    await extension.awaitMessage("next");
    await extension.awaitMessage("next");
  }

  // During this test, at most one "onclick" attribute is expected at any time.
  for (let pageOne of ["background", "tab"]) {
    for (let pageTwo of ["background", "tab"]) {
      info(`Testing with menu created by ${pageOne} and updated by ${pageTwo}`);
      extension.sendMessage(pageOne, "create-with-onclick");
      await extension.awaitMessage("next");

      // Test that update without onclick attribute does not clear the existing
      // onclick handler.
      extension.sendMessage(pageTwo, "update-without-onclick");
      await extension.awaitMessage("next");
      await clickContextMenu();
      let clickCounts = await getCounts(pageOne);
      is(clickCounts.old, 1, `Original onclick should still be present in ${pageOne}`);
      is(clickCounts.new, 0, `Not expecting any new handlers in ${pageOne}`);
      if (pageOne !== pageTwo) {
        clickCounts = await getCounts(pageTwo);
        is(clickCounts.old, 0, `Not expecting any handlers in ${pageTwo}`);
        is(clickCounts.new, 0, `Not expecting any new handlers in ${pageTwo}`);
      }
      await resetCounts();

      // Test that update with onclick handler in a different page clears the
      // existing handler and activates the new onclick handler.
      extension.sendMessage(pageTwo, "update-with-onclick");
      await extension.awaitMessage("next");
      await clickContextMenu();
      clickCounts = await getCounts(pageOne);
      is(clickCounts.old, 0, `Original onclick should be gone from ${pageOne}`);
      if (pageOne !== pageTwo) {
        is(clickCounts.new, 0, `Still not expecting new handlers in ${pageOne}`);
      }
      clickCounts = await getCounts(pageTwo);
      if (pageOne !== pageTwo) {
        is(clickCounts.old, 0, `Not expecting an old onclick in ${pageTwo}`);
      }
      is(clickCounts.new, 1, `New onclick should be triggered in ${pageTwo}`);
      await resetCounts();

      // Test that updating the handler (different again from the last `update`
      // call, but the same as the `create` call) clears the existing handler
      // and activates the new onclick handler.
      extension.sendMessage(pageOne, "update-with-onclick");
      await extension.awaitMessage("next");
      await clickContextMenu();
      clickCounts = await getCounts(pageOne);
      is(clickCounts.new, 1, `onclick should be triggered in ${pageOne}`);
      if (pageOne !== pageTwo) {
        clickCounts = await getCounts(pageTwo);
        is(clickCounts.new, 0, `onclick should be gone from ${pageTwo}`);
      }
      await resetCounts();

      // Test that removing the context menu and recreating it with the same ID
      // (in a different context) does not leave behind any onclick handlers.
      extension.sendMessage(pageTwo, "remove");
      await extension.awaitMessage("next");
      extension.sendMessage(pageTwo, "create-without-onclick");
      await extension.awaitMessage("next");
      await clickContextMenu();
      clickCounts = await getCounts(pageOne);
      is(clickCounts.new, 0, `Did not expect any click handlers in ${pageOne}`);
      if (pageOne !== pageTwo) {
        clickCounts = await getCounts(pageTwo);
        is(clickCounts.new, 0, `Did not expect any click handlers in ${pageTwo}`);
      }
      await resetCounts();

      // Remove context menu for the next iteration of the test. And just to get
      // more coverage, let's use removeAll instead of remove.
      extension.sendMessage(pageOne, "removeAll");
      await extension.awaitMessage("next");
    }
  }
  await extension.unload();
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_onclick_modifiers() {
  const manifest = {
    permissions: ["contextMenus"],
  };

  function background() {
    function onclick(info) {
      browser.test.sendMessage("click", info);
    }
    browser.contextMenus.create({contexts: ["all"], title: "modify", onclick});
    browser.test.sendMessage("ready");
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  async function click(modifiers = {}) {
    const menu = await openContextMenu();
    const items = menu.getElementsByAttribute("label", "modify");
    is(items.length, 1, "Got exactly one context menu item");
    await closeExtensionContextMenu(items[0], modifiers);
    return extension.awaitMessage("click");
  }

  const plain = await click();
  is(plain.modifiers.length, 0, "modifiers array empty with a plain click");

  const shift = await click({shiftKey: true});
  is(shift.modifiers.join(), "Shift", "Correct modifier: Shift");

  const ctrl = await click({ctrlKey: true});
  if (AppConstants.platform !== "macosx") {
    is(ctrl.modifiers.join(), "Ctrl", "Correct modifier: Ctrl");
  } else {
    is(ctrl.modifiers.sort().join(), "Ctrl,MacCtrl", "Correct modifier: Ctrl (and MacCtrl)");

    const meta = await click({metaKey: true});
    is(meta.modifiers.join(), "Command", "Correct modifier: Command");
  }

  const altShift = await click({altKey: true, shiftKey: true});
  is(altShift.modifiers.sort().join(), "Alt,Shift", "Correct modifiers: Shift+Alt");

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
