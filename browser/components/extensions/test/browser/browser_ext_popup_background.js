/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

async function testPanel(browser, standAlone, background_check) {
  let panel = getPanelForNode(browser);

  let checkBackground = (background = null) => {
    if (!standAlone) {
      return;
    }

    is(
      getComputedStyle(panel.panelContent).backgroundColor,
      background,
      "Content should have correct background"
    );
  };

  function getBackground(browser) {
    return SpecialPowers.spawn(browser, [], async function () {
      return content.windowUtils.canvasBackgroundColor;
    });
  }

  let setBackground = color => {
    content.document.body.style.backgroundColor = color;
  };

  await new Promise(resolve => setTimeout(resolve, 100));

  info("Test that initial background color is applied");
  let initialBackground = await getBackground(browser);
  checkBackground(initialBackground);
  background_check(initialBackground);

  info("Test that dynamically-changed background color is applied");
  await alterContent(browser, setBackground, "black");
  checkBackground(await getBackground(browser));

  info("Test that non-opaque background color results in default styling");
  await alterContent(browser, setBackground, "rgba(1, 2, 3, .9)");
}

add_task(async function testPopupBackground() {
  let testCases = [
    {
      browser_style: false,
      popup: `
        <!doctype html>
        <body style="width: 100px; height: 100px; background-color: green">
        </body>
      `,
      background_check: function (bg) {
        is(bg, "rgb(0, 128, 0)", "Initial background should be green");
      },
    },
    {
      browser_style: false,
      popup: `
        <!doctype html>
        <body style="width: 100px; height: 100px"">
        </body>
      `,
      background_check: function (bg) {
        is(bg, "rgb(255, 255, 255)", "Initial background should be white");
      },
    },
    {
      browser_style: false,
      popup: `
        <!doctype html>
        <meta name=color-scheme content=light>
        <body style="width: 100px; height: 100px;">
        </body>
      `,
      background_check: function (bg) {
        is(bg, "rgb(255, 255, 255)", "Initial background should be white");
      },
    },
    {
      browser_style: false,
      popup: `
        <!doctype html>
        <meta name=color-scheme content=dark>
        <body style="width: 100px; height: 100px;">
        </body>
      `,
      background_check: function (bg) {
        isnot(
          bg,
          "rgb(255, 255, 255)",
          "Initial background should not be white"
        );
      },
    },
  ];
  for (let { browser_style, popup, background_check } of testCases) {
    info(`Testing browser_style: ${browser_style} popup: ${popup}`);
    let extension = ExtensionTestUtils.loadExtension({
      background() {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          browser.pageAction.show(tabs[0].id);
        });
      },

      manifest: {
        browser_action: {
          default_popup: "popup.html",
          default_area: "navbar",
          browser_style,
        },

        page_action: {
          default_popup: "popup.html",
          browser_style,
        },
      },

      files: {
        "popup.html": popup,
      },
    });

    await extension.startup();

    {
      info("Test stand-alone browserAction popup");

      clickBrowserAction(extension);
      let browser = await awaitExtensionPanel(extension);
      await testPanel(browser, true, background_check);
      await closeBrowserAction(extension);
    }

    {
      info("Test menu panel browserAction popup");

      let widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, getCustomizableUIPanelID());

      clickBrowserAction(extension);
      let browser = await awaitExtensionPanel(extension);
      await testPanel(browser, false, background_check);
      await closeBrowserAction(extension);
    }

    {
      info("Test pageAction popup");

      clickPageAction(extension);
      let browser = await awaitExtensionPanel(extension);
      await testPanel(browser, true, background_check);
      await closePageAction(extension);
    }

    await extension.unload();
  }
});
