/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let getExtension = () => {
  return ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        browser.pageAction.show(tabs[0].id)
          .then(() => { browser.test.sendMessage("pageAction ready"); });
      });
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

add_task(function* testStandaloneBrowserAction() {
  info("Test stand-alone browserAction popup");

  let extension = getExtension();
  yield extension.startup();
  yield extension.awaitMessage("pageAction ready");

  clickBrowserAction(extension);
  let browser = yield awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  yield extension.unload();

  is(panel.parentNode, null, "Panel should be removed from the document");
});

add_task(function* testMenuPanelBrowserAction() {
  let extension = getExtension();
  yield extension.startup();
  yield extension.awaitMessage("pageAction ready");

  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, CustomizableUI.AREA_PANEL);

  clickBrowserAction(extension);
  let browser = yield awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  yield extension.unload();

  is(panel.state, "closed", "Panel should be closed");
});

add_task(function* testPageAction() {
  let extension = getExtension();
  yield extension.startup();
  yield extension.awaitMessage("pageAction ready");

  clickPageAction(extension);
  let browser = yield awaitExtensionPanel(extension);
  let panel = getPanelForNode(browser);

  yield extension.unload();

  is(panel.parentNode, null, "Panel should be removed from the document");
});
