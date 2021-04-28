/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer({ hosts: ["www.example.com"] });
server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Type", "text/html; charset=UTF-8", false);
  response.write(`<!DOCTYPE html>
  <html>
    <head>
      <meta charset="utf-8">
    </head>
    <body>
      This is example.com page content
    </body>
  </html>
  `);
});

add_task(async function containerIsolation_restricted() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.userContextIsolation.enabled", true],
      ["privacy.userContext.enabled", true],
    ],
  });

  let helperExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "cookies"],
    },

    async background() {
      browser.test.onMessage.addListener(async message => {
        let tab;
        if (message?.subject !== "createTab") {
          browser.test.fail(
            `Unexpected test message received: ${JSON.stringify(message)}`
          );
          return;
        }

        tab = await browser.tabs.create({
          url: message.data.url,
          cookieStoreId: message.data.cookieStoreId,
        });
        browser.test.sendMessage("tabCreated", tab.id);
        browser.test.assertEq(
          message.data.cookieStoreId,
          tab.cookieStoreId,
          "Created tab is associated with the expected cookieStoreId"
        );
      });
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "cookies", "<all_urls>", "tabHide"],
    },
    async background() {
      const [restrictedTab, unrestrictedTab] = await new Promise(resolve => {
        browser.test.onMessage.addListener(message => resolve(message));
      });

      // Check that print preview method fails
      await browser.test.assertRejects(
        browser.tabs.printPreview(),
        /Cannot access activeTab/,
        "should refuse to print a preview of the tab for the container which doesn't have permission"
      );

      // Check that save As PDF method fails
      await browser.test.assertRejects(
        browser.tabs.saveAsPDF({}),
        /Cannot access activeTab/,
        "should refuse to save as PDF of the tab for the container which doesn't have permission"
      );

      // Check that create method fails
      await browser.test.assertRejects(
        browser.tabs.create({ cookieStoreId: "firefox-container-1" }),
        /Cannot access firefox-container-1/,
        "should refuse to create container tab for the container which doesn't have permission"
      );

      // Check that detect language method fails
      await browser.test.assertRejects(
        browser.tabs.detectLanguage(restrictedTab),
        /Invalid tab ID/,
        "should refuse to detect language of a tab for the container which doesn't have permission"
      );

      // Check that move tabs method fails
      await browser.test.assertRejects(
        browser.tabs.move(restrictedTab, { index: 1 }),
        /Invalid tab ID/,
        "should refuse to move tab for the container which doesn't have permission"
      );

      // Check that duplicate method fails.
      await browser.test.assertRejects(
        browser.tabs.duplicate(restrictedTab),
        /Invalid tab ID:/,
        "should refuse to duplicate tab for the container which doesn't have permission"
      );

      // Check that captureTab method fails.
      await browser.test.assertRejects(
        browser.tabs.captureTab(restrictedTab),
        /Invalid tab ID/,
        "should refuse to capture the tab for the container which doesn't have permission"
      );

      // Check that discard method fails.
      await browser.test.assertRejects(
        browser.tabs.discard([restrictedTab]),
        /Invalid tab ID/,
        "should refuse to discard the tab for the container which doesn't have permission "
      );

      // Check that get method fails.
      await browser.test.assertRejects(
        browser.tabs.get(restrictedTab),
        /Invalid tab ID/,
        "should refuse to get the tab for the container which doesn't have permissiond"
      );

      // Check that highlight method fails.
      await browser.test.assertRejects(
        browser.tabs.highlight({ populate: true, tabs: [restrictedTab] }),
        `No tab at index: ${restrictedTab}`,
        "should refuse to highlight the tab for the container which doesn't have permission"
      );

      // Test for moveInSuccession method of tab

      await browser.test.assertRejects(
        browser.tabs.moveInSuccession([restrictedTab]),
        /Invalid tab ID/,
        "should refuse to moveInSuccession for the container which doesn't have permission"
      );

      // Check that executeScript method fails.
      await browser.test.assertRejects(
        browser.tabs.executeScript(restrictedTab, { frameId: 0 }),
        /Invalid tab ID/,
        "should refuse to execute a script of the tab for the container which doesn't have permission"
      );

      // Check that getZoom method fails.

      await browser.test.assertRejects(
        browser.tabs.getZoom(restrictedTab),
        /Invalid tab ID/,
        "should refuse to zoom the tab for the container which doesn't have permission"
      );

      // Check that getZoomSetting method fails.
      await browser.test.assertRejects(
        browser.tabs.getZoomSettings(restrictedTab),
        /Invalid tab ID/,
        "should refuse the setting of zoom of the tab for the container which doesn't have permission"
      );

      //Test for hide method of tab
      await browser.test.assertRejects(
        browser.tabs.hide(restrictedTab),
        /Invalid tab ID/,
        "should refuse to hide a tab for the container which doesn't have permission"
      );

      // Check that insertCSS method fails.
      await browser.test.assertRejects(
        browser.tabs.insertCSS(restrictedTab, { frameId: 0 }),
        /Invalid tab ID/,
        "should refuse to insert a stylesheet to the tab for the container which doesn't have permission"
      );

      // Check that removeCSS method fails.
      await browser.test.assertRejects(
        browser.tabs.removeCSS(restrictedTab, { frameId: 0 }),
        /Invalid tab ID/,
        "should refuse to remove a stylesheet to the tab for the container which doesn't have permission"
      );

      //Test for show method of tab
      await browser.test.assertRejects(
        browser.tabs.show([restrictedTab]),
        /Invalid tab ID/,
        "should refuse to show the tab for the container which doesn't have permission"
      );

      // Check that toggleReaderMode method fails.

      await browser.test.assertRejects(
        browser.tabs.toggleReaderMode(restrictedTab),
        /Invalid tab ID/,
        "should refuse to toggle reader mode in the tab for the container which doesn't have permission"
      );

      // Check that setZoom method fails.
      await browser.test.assertRejects(
        browser.tabs.setZoom(restrictedTab, 0),
        /Invalid tab ID/,
        "should refuse to set zoom of the tab for the container which doesn't have permission"
      );

      // Check that setZoomSettings method fails.
      await browser.test.assertRejects(
        browser.tabs.setZoomSettings(restrictedTab, { defaultZoomFactor: 1 }),
        /Invalid tab ID/,
        "should refuse to set zoom setting of the tab for the container which doesn't have permission"
      );

      // Check that goBack method fails.

      await browser.test.assertRejects(
        browser.tabs.goBack(restrictedTab),
        /Invalid tab ID/,
        "should refuse to go back to the tab for the container which doesn't have permission"
      );

      // Check that goForward method fails.

      await browser.test.assertRejects(
        browser.tabs.goForward(restrictedTab),
        /Invalid tab ID/,
        "should refuse to go forward to the tab for the container which doesn't have permission"
      );

      // Check that update method fails.
      await browser.test.assertRejects(
        browser.tabs.update(restrictedTab, { highlighted: true }),
        /Invalid tab ID/,
        "should refuse to update the tab for the container which doesn't have permission"
      );

      // Check that reload method fails.
      await browser.test.assertRejects(
        browser.tabs.reload(restrictedTab),
        /Invalid tab ID/,
        "should refuse to reload tab for the container which doesn't have permission"
      );

      //Test for warmup method of tab
      await browser.test.assertRejects(
        browser.tabs.warmup(restrictedTab),
        /Invalid tab ID/,
        "should refuse to warmup a tab for the container which doesn't have permission"
      );

      let gettab = await browser.tabs.get(unrestrictedTab);
      browser.test.assertEq(
        gettab.cookieStoreId,
        "firefox-container-2",
        "get tab should open"
      );

      let lang = await browser.tabs.detectLanguage(unrestrictedTab);
      await browser.test.assertEq(
        "en",
        lang,
        "English document should be detected"
      );

      let duptab = await browser.tabs.duplicate(unrestrictedTab);

      browser.test.assertEq(
        duptab.cookieStoreId,
        "firefox-container-2",
        "duplicated tab should open"
      );
      await browser.tabs.remove(duptab.id);

      let moved = await browser.tabs.move(unrestrictedTab, {
        index: 0,
      });
      browser.test.assertEq(moved.length, 1, "move() returned no moved tab");

      //Test for query method of tab
      let tabs = await browser.tabs.query({
        cookieStoreId: "firefox-container-1",
      });
      await browser.test.assertEq(
        0,
        tabs.length,
        "should not return restricted container's tab"
      );

      tabs = await browser.tabs.query({});
      await browser.test.assertEq(
        tabs
          .map(tab => tab.cookieStoreId)
          .sort()
          .join(","),
        "firefox-container-2,firefox-default",
        "should return two tabs - firefox-default and firefox-container-2"
      );

      // Check that remove method fails.

      await browser.test.assertRejects(
        browser.tabs.remove([restrictedTab]),
        /Invalid tab ID/,
        "should refuse to remove tab for the container which doesn't have permission"
      );

      let removedPromise = new Promise(resolve => {
        browser.tabs.onRemoved.addListener(tabId => {
          browser.test.assertEq(unrestrictedTab, tabId, "expected remove tab");
          resolve();
        });
      });
      await browser.tabs.remove(unrestrictedTab);
      await removedPromise;

      browser.test.sendMessage("done");
    },
  });

  await helperExtension.startup();

  helperExtension.sendMessage({
    subject: "createTab",
    data: {
      url: "http://www.example.com/",
      cookieStoreId: "firefox-container-2",
    },
  });
  const unrestrictedTab = await helperExtension.awaitMessage("tabCreated");

  helperExtension.sendMessage({
    subject: "createTab",
    data: {
      url: "http://www.example.com/",
      cookieStoreId: "firefox-container-1",
    },
  });
  const restrictedTab = await helperExtension.awaitMessage("tabCreated");

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.userContextIsolation.defaults.restricted", "[1]"]],
  });

  await extension.startup();
  extension.sendMessage([restrictedTab, unrestrictedTab]);

  await extension.awaitMessage("done");
  await extension.unload();
  await helperExtension.unload();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});
