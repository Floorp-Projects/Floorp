/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testPopupBorderRadius() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        browser.pageAction.show(tabs[0].id);
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
        <html>
          <head><meta charset="utf-8"></head>
          <body style="width: 100px; height: 100px;"></body>
        </html>`,
    },
  });

  await extension.startup();

  async function testPanel(browser, standAlone = true) {
    let panel = getPanelForNode(browser);
    let arrowContent = document.getAnonymousElementByAttribute(panel, "class", "panel-arrowcontent");

    let panelStyle = getComputedStyle(arrowContent);

    let viewNode = browser.parentNode === panel ? browser : browser.parentNode;
    let viewStyle = getComputedStyle(viewNode);

    let props = ["borderTopLeftRadius", "borderTopRightRadius",
                 "borderBottomRightRadius", "borderBottomLeftRadius"];

    /* eslint-disable mozilla/no-cpows-in-tests */
    let bodyStyle = await ContentTask.spawn(browser, props, async function(props) {
      let bodyStyle = content.getComputedStyle(content.document.body);

      return new Map(props.map(prop => [prop, bodyStyle[prop]]));
    });
    /* eslint-enable mozilla/no-cpows-in-tests */

    for (let prop of props) {
      if (standAlone) {
        is(viewStyle[prop], panelStyle[prop], `Panel and view ${prop} should be the same`);
        is(bodyStyle.get(prop), panelStyle[prop], `Panel and body ${prop} should be the same`);
      } else {
        is(viewStyle[prop], "0px", `View node ${prop} should be 0px`);
        is(bodyStyle.get(prop), "0px", `Body node ${prop} should be 0px`);
      }
    }
  }

  {
    info("Test stand-alone browserAction popup");

    clickBrowserAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser);
    await closeBrowserAction(extension);
  }

  {
    info("Test menu panel browserAction popup");

    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, getCustomizableUIPanelID());

    clickBrowserAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser, false);
    await closeBrowserAction(extension);
  }

  {
    info("Test pageAction popup");

    clickPageAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser);
    await closePageAction(extension);
  }

  await extension.unload();
});
