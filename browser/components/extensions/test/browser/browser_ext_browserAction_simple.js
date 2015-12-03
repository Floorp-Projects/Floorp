/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": function() {
        browser.runtime.sendMessage("from-popup");
      },
    },

    background: function() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "from-popup", "correct message received");
        browser.test.sendMessage("popup");
      });
    },
  });

  yield extension.startup();

  let widgetId = makeWidgetId(extension.id) + "-browser-action";
  let node = CustomizableUI.getWidget(widgetId).forWindow(window).node;

  // Do this a few times to make sure the pop-up is reloaded each time.
  for (let i = 0; i < 3; i++) {
    let evt = new CustomEvent("command", {
      bubbles: true,
      cancelable: true,
    });
    node.dispatchEvent(evt);

    yield extension.awaitMessage("popup");

    let panel = node.querySelector("panel");
    if (panel) {
      panel.hidePopup();
    }
  }

  yield extension.unload();
});
