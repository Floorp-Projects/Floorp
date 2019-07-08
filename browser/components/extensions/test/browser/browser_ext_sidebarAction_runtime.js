"use strict";

function background() {
  browser.runtime.onConnect.addListener(port => {
    browser.test.assertEq(port.name, "ernie", "port name correct");
    port.onDisconnect.addListener(() => {
      browser.test.assertEq(
        null,
        port.error,
        "The port is implicitly closed without errors when the other context unloads"
      );
      port.disconnect();
      browser.test.sendMessage("disconnected");
    });
    browser.test.sendMessage("connected");
  });
}

let extensionData = {
  background,
  manifest: {
    sidebar_action: {
      default_panel: "sidebar.html",
    },
  },
  useAddonManager: "temporary",

  files: {
    "sidebar.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/>
      <script src="sidebar.js"></script>
      </head>
      <body>
      A Test Sidebar
      </body></html>
    `,

    "sidebar.js": function() {
      window.onload = () => {
        browser.runtime.connect({ name: "ernie" });
      };
    },
  },
};

add_task(async function test_sidebar_disconnect() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  let connected = extension.awaitMessage("connected");
  await extension.startup();
  await connected;

  // Bug 1445080 fixes currentURI, test to avoid future breakage.
  let currentURI = window.SidebarUI.browser.contentDocument.getElementById(
    "webext-panels-browser"
  ).currentURI;
  is(currentURI.scheme, "moz-extension", "currentURI is set correctly");

  // switching sidebar to another extension
  let extension2 = ExtensionTestUtils.loadExtension(extensionData);
  let switched = Promise.all([
    extension.awaitMessage("disconnected"),
    extension2.awaitMessage("connected"),
  ]);
  await extension2.startup();
  await switched;

  // switching sidebar to built-in sidebar
  let disconnected = extension2.awaitMessage("disconnected");
  window.SidebarUI.show("viewBookmarksSidebar");
  await disconnected;

  await extension.unload();
  await extension2.unload();
});
