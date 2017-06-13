/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let getExtension = () => {
  return ExtensionTestUtils.loadExtension({
    background: async function() {
      let [tab] = await browser.tabs.query({active: true, currentWindow: true});
      await browser.pageAction.show(tab.id);
      browser.test.sendMessage("pageAction ready");
    },

    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": false,
      },

      "page_action": {
        "default_popup": "popup.html",
        "browser_style": false,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html>
        <html><head><meta charset="utf-8"></head></html>`,
    },
  });
};

add_task(async function testStandaloneBrowserAction() {
  info("Test stand-alone browserAction popup");

  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("pageAction ready");

  clickBrowserAction(extension);
  let browser = await awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  await extension.unload();

  is(panel.parentNode, null, "Panel should be removed from the document");
});

add_task(async function testMenuPanelBrowserAction() {
  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("pageAction ready");

  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, getCustomizableUIPanelID());

  clickBrowserAction(extension);
  let browser = await awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  await extension.unload();

  is(panel.state, "closed", "Panel should be closed");
});

add_task(async function testPageAction() {
  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("pageAction ready");

  clickPageAction(extension);
  let browser = await awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  await extension.unload();

  is(panel.parentNode, null, "Panel should be removed from the document");
});
