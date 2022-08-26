/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function promisePanelElementShown(win, panel) {
  return new Promise((resolve, reject) => {
    let timeoutId = win.setTimeout(() => {
      reject("Panel did not show within 20 seconds.");
    }, 20000);
    function onPanelOpen(e) {
      panel.removeEventListener("popupshown", onPanelOpen);
      win.clearTimeout(timeoutId);
      resolve();
    }
    panel.addEventListener("popupshown", onPanelOpen);
  });
}

add_task(async function testPopupBorderRadius() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        browser.pageAction.show(tabs[0].id);
      });
    },

    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: false,
      },

      page_action: {
        default_popup: "popup.html",
        browser_style: false,
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

  let widget = getBrowserActionWidget(extension);
  // If the panel doesn't allows embedding in subview then
  // radius will be 0, otherwise 8.  In practice we always
  // disallow subview.
  let expectedRadius = widget.disallowSubView ? "8px" : "0px";

  async function testPanel(browser, standAlone = true) {
    let panel = getPanelForNode(browser);
    let arrowContent = panel.panelContent;

    let panelStyle = getComputedStyle(arrowContent);
    is(
      panelStyle.overflow,
      "hidden",
      "overflow is not hidden, thus it doesn't clip"
    );

    let stack = browser.parentNode;
    let viewNode = stack.parentNode === panel ? browser : stack.parentNode;
    let viewStyle = getComputedStyle(viewNode);

    let props = [
      "borderTopLeftRadius",
      "borderTopRightRadius",
      "borderBottomRightRadius",
      "borderBottomLeftRadius",
    ];

    let bodyStyle = await SpecialPowers.spawn(browser, [props], async function(
      props
    ) {
      let bodyStyle = content.getComputedStyle(content.document.body);

      return new Map(props.map(prop => [prop, bodyStyle[prop]]));
    });

    for (let prop of props) {
      if (standAlone) {
        is(
          viewStyle[prop],
          panelStyle[prop],
          `Panel and view ${prop} should be the same`
        );
        is(
          bodyStyle.get(prop),
          panelStyle[prop],
          `Panel and body ${prop} should be the same`
        );
      } else {
        is(viewStyle[prop], expectedRadius, `View node ${prop} should be 0px`);
        is(
          bodyStyle.get(prop),
          expectedRadius,
          `Body node ${prop} should be 0px`
        );
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
    info("Test overflowed browserAction popup");
    const kForceOverflowWidthPx = 450;
    let overflowPanel = document.getElementById("widget-overflow");

    let originalWindowWidth = window.outerWidth;
    let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
    ok(
      !navbar.hasAttribute("overflowing"),
      "Should start with a non-overflowing toolbar."
    );
    window.resizeTo(kForceOverflowWidthPx, window.outerHeight);

    await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
    ok(
      navbar.hasAttribute("overflowing"),
      "Should have an overflowing toolbar."
    );

    let chevron = document.getElementById("nav-bar-overflow-button");
    let shownPanelPromise = promisePanelElementShown(window, overflowPanel);
    chevron.click();
    await shownPanelPromise;

    clickBrowserAction(extension);
    let browser = await awaitExtensionPanel(extension);

    is(
      overflowPanel.state,
      "closed",
      "The widget overflow panel should not be open."
    );

    await testPanel(browser, false);
    await closeBrowserAction(extension);

    window.resizeTo(originalWindowWidth, window.outerHeight);
    await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
    ok(
      !navbar.hasAttribute("overflowing"),
      "Should not have an overflowing toolbar."
    );
  }

  {
    info("Test menu panel browserAction popup");

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
