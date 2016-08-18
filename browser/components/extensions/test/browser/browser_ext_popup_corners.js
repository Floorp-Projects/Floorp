/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* awaitPanel(extension, win = window) {
  let {target} = yield BrowserTestUtils.waitForEvent(win.document, "load", true, (event) => {
    return event.target.location && event.target.location.href.endsWith("popup.html");
  });

  // Wait for the browser resize code, which is triggered asynchronously by the
  // load event, to run.
  yield new Promise(resolve => setTimeout(resolve, 10));

  return target.defaultView
               .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell)
               .chromeEventHandler;
}

add_task(function* testPopupBorderRadius() {
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

  yield extension.startup();

  function* testPanel(browser, standAlone = true) {
    let panel = getPanelForNode(browser);
    let arrowContent = document.getAnonymousElementByAttribute(panel, "class", "panel-arrowcontent");

    let panelStyle = getComputedStyle(arrowContent);

    let viewNode = browser.parentNode === panel ? browser : browser.parentNode;
    let viewStyle = getComputedStyle(viewNode);

    for (let prop of ["borderTopLeftRadius", "borderTopRightRadius",
                      "borderBottomRightRadius", "borderBottomLeftRadius"]) {
      if (standAlone) {
        is(viewStyle[prop], panelStyle[prop], `Panel and view ${prop} should be the same`);
      } else {
        is(viewStyle[prop], "0px", `View node ${prop} should be 0px`);
      }
    }
  }

  {
    info("Test stand-alone browserAction popup");

    clickBrowserAction(extension);
    let browser = yield awaitPanel(extension);
    yield testPanel(browser);
    yield closeBrowserAction(extension);
  }

  {
    info("Test menu panel browserAction popup");

    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, CustomizableUI.AREA_PANEL);

    clickBrowserAction(extension);
    let browser = yield awaitPanel(extension);
    yield testPanel(browser, false);
    yield closeBrowserAction(extension);
  }

  {
    info("Test pageAction popup");

    clickPageAction(extension);
    let browser = yield awaitPanel(extension);
    yield testPanel(browser);
    yield closePageAction(extension);
  }

  yield extension.unload();
});
