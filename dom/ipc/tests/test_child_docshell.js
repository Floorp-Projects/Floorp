"use strict";

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

add_task(async function test() {
  let page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  await new Promise(resolve => {
    let mm = page.browser.messageManager;
    mm.addMessageListener("chromeEventHandler", function(msg) {
      var result = msg.json;
      equal(
        result.processType,
        Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT,
        "The frame script is running in a real distinct child process"
      );
      ok(
        result.hasCorrectInterface,
        "docshell.chromeEventHandler has EventTarget interface"
      );
    });

    mm.addMessageListener("DOMWindowCreatedReceived", function(msg) {
      ok(true, "the chrome event handler looks functional");
      var result = msg.json;
      ok(
        result.stableChromeEventHandler,
        "docShell.chromeEventHandler is stable"
      );
      ok(result.iframeHasNewDocShell, "iframe spawns a new docShell");
      ok(
        result.iframeHasSameChromeEventHandler,
        "but iframe has the same chrome event handler"
      );
      resolve();
    });

    // Inject a frame script in the child process:
    page.loadFrameScript(async function() {
      /* eslint-env mozilla/frame-script */
      var chromeEventHandler = docShell.chromeEventHandler;
      sendAsyncMessage("chromeEventHandler", {
        processType: Services.appinfo.processType,
        hasCorrectInterface:
          chromeEventHandler && EventTarget.isInstance(chromeEventHandler),
      });

      /*
        Ensure that this chromeEventHandler actually works,
        by creating a new window and listening for its DOMWindowCreated event
      */
      chromeEventHandler.addEventListener("DOMWindowCreated", function listener(
        evt
      ) {
        if (evt.target == content.document) {
          return;
        }
        chromeEventHandler.removeEventListener("DOMWindowCreated", listener);
        let new_win = evt.target.defaultView;
        let new_docShell = new_win.docShell;
        sendAsyncMessage("DOMWindowCreatedReceived", {
          stableChromeEventHandler:
            chromeEventHandler === docShell.chromeEventHandler,
          iframeHasNewDocShell: new_docShell !== docShell,
          iframeHasSameChromeEventHandler:
            new_docShell.chromeEventHandler === chromeEventHandler,
        });
      });

      if (content.document.readyState != "complete") {
        await new Promise(res =>
          addEventListener("load", res, { once: true, capture: true })
        );
      }

      let iframe = content.document.createElement("iframe");
      iframe.setAttribute("src", "data:text/html,foo");
      content.document.documentElement.appendChild(iframe);
    });
  });

  await page.close();
});
