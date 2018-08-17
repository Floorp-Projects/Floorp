/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function menuInShadowDOM() {
  Services.prefs.setBoolPref("dom.webcomponents.shadowdom.enabled", true);
  registerCleanupFunction(() => Services.prefs.clearUserPref("dom.webcomponents.shadowdom.enabled"));

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;

  async function background() {
    browser.menus.onShown.addListener(async (info, tab) => {
      browser.test.assertTrue(Number.isInteger(info.targetElementId), `${info.targetElementId} should be an integer`);
      browser.test.assertEq("all,link", info.contexts.sort().join(","), "Expected context");
      browser.test.assertEq("http://example.com/?shadowlink", info.linkUrl, "Menu target should be a link in the shadow DOM");

      let code = `{
        try {
          let elem = browser.menus.getTargetElement(${info.targetElementId});
          browser.test.assertTrue(elem, "Shadow element must be found");
          browser.test.assertEq("http://example.com/?shadowlink", elem.href, "Element is a link in shadow DOM " - elem.outerHTML);
        } catch (e) {
          browser.test.fail("Unexpected error in getTargetElement: " + e);
        }
      }`;
      await browser.tabs.executeScript(tab.id, {code});
      browser.test.sendMessage("onShownMenuAndCheckedInfo", info.targetElementId);
    });

    // Ensure that onShown is registered (workaround for bug 1300234):
    await browser.menus.removeAll();
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "http://mochi.test/*"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  async function testShadowMenu(setupMenuTarget) {
    await openContextMenu(setupMenuTarget);
    await extension.awaitMessage("onShownMenuAndCheckedInfo");
    await closeContextMenu();
  }

  info("Clicking in open shadow root");
  await testShadowMenu(() => {
    let doc = content.document;
    doc.body.innerHTML = `<div></div>`;
    let host = doc.body.firstElementChild.attachShadow({mode: "open"});
    host.innerHTML = `<a href="http://example.com/?shadowlink">Test open</a>`;
    content.testTarget = host.firstElementChild;
    return content.testTarget;
  });

  info("Clicking in closed shadow root");
  await testShadowMenu(() => {
    let doc = content.document;
    doc.body.innerHTML = `<div></div>`;
    let host = doc.body.firstElementChild.attachShadow({mode: "closed"});
    host.innerHTML = `<a href="http://example.com/?shadowlink">Test closed</a>`;
    content.testTarget = host.firstElementChild;
    return content.testTarget;
  });

  info("Clicking in nested shadow DOM");
  await testShadowMenu(() => {
    let doc = content.document;
    let host;
    for (let container = doc.body, i = 0; i < 10; ++i) {
      container.innerHTML = `<div id="level"></div>`;
      host = container.firstElementChild.attachShadow({mode: "open"});
      container = host;
    }
    host.innerHTML = `<a href="http://example.com/?shadowlink">Test nested</a>`;
    content.testTarget = host.firstElementChild;
    return content.testTarget;
  });

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
