let {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm");

function makeWidgetId(id)
{
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html"
      }
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
      }
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
      cancelable: true
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
