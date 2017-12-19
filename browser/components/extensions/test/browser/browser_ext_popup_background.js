/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function testPopupBackground() {
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
          <head>
            <meta charset="utf-8">
          </head>
          <body style="width: 100px; height: 100px; background-color: green;">
          </body>
        </html>`,
    },
  });

  await extension.startup();

  async function testPanel(browser, standAlone) {
    let panel = getPanelForNode(browser);
    let arrowContent = document.getAnonymousElementByAttribute(panel, "class", "panel-arrowcontent");
    let arrow = document.getAnonymousElementByAttribute(panel, "anonid", "arrow");

    let checkArrow = (background = null) => {
      if (background == null || !standAlone) {
        ok(!arrow.style.hasOwnProperty("fill"), "Arrow fill should be the default one");
        return;
      }

      is(getComputedStyle(arrowContent).backgroundColor, background, "Arrow content should have correct background");
      is(getComputedStyle(arrow).fill, background, "Arrow should have correct background");
    };

    function getBackground(browser) {
      return ContentTask.spawn(browser, null, async function() {
        return content.getComputedStyle(content.document.body)
                      .backgroundColor;
      });
    }

    /* eslint-disable mozilla/no-cpows-in-tests */
    let setBackground = color => {
      content.document.body.style.backgroundColor = color;
    };
    /* eslint-enable mozilla/no-cpows-in-tests */

    await new Promise(resolve => setTimeout(resolve, 100));

    info("Test that initial background color is applied");

    checkArrow(await getBackground(browser));

    info("Test that dynamically-changed background color is applied");

    await alterContent(browser, setBackground, "black");

    checkArrow(await getBackground(browser));

    info("Test that non-opaque background color results in default styling");

    await alterContent(browser, setBackground, "rgba(1, 2, 3, .9)");

    checkArrow(null);
  }

  {
    info("Test stand-alone browserAction popup");

    clickBrowserAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser, true);
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
    await testPanel(browser, true);
    await closePageAction(extension);
  }

  await extension.unload();
});
