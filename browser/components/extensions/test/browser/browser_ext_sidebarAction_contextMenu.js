/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let extData = {
  manifest: {
    "permissions": ["contextMenus"],
    "sidebar_action": {
      "default_panel": "sidebar.html",
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
      <span id="text">A Test Sidebar</span>
      <img id="testimg" src="data:image/svg+xml,<svg></svg>" height="10" width="10">
      </body></html>
    `,

    "sidebar.js": function() {
      window.onload = () => {
        browser.test.sendMessage("sidebar");
      };
    },
  },

  background: function() {
    browser.contextMenus.create({
      id: "clickme-page",
      title: "Click me!",
      contexts: ["all"],
      onclick(info, tab) {
        browser.test.sendMessage("menu-click", tab);
      },
    });
  },
};

let contextMenuItems = {
  "context-navigation": "hidden",
  "context-sep-navigation": "hidden",
  "context-viewsource": "",
  "context-viewinfo": "",
  "inspect-separator": "hidden",
  "context-inspect": "hidden",
  "context-bookmarkpage": "hidden",
};

add_task(async function sidebar_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");

  let contentAreaContextMenu = await openContextMenuInSidebar();
  let item = contentAreaContextMenu.getElementsByAttribute("label", "Click me!");
  is(item.length, 1, "contextMenu item for page was found");

  item[0].click();
  await closeContextMenu(contentAreaContextMenu);
  let tab = await extension.awaitMessage("menu-click");
  is(tab, null, "tab argument is optional, and missing in clicks from sidebars");

  await extension.unload();
});


add_task(async function sidebar_contextmenu_hidden_items() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");

  let contentAreaContextMenu = await openContextMenuInSidebar("#text");

  let item, state;
  for (const itemID in contextMenuItems) {
    item = contentAreaContextMenu.querySelector(`#${itemID}`);
    state = contextMenuItems[itemID];

    if (state !== "") {
      ok(item[state], `${itemID} is ${state}`);

      if (state !== "hidden") {
        ok(!item.hidden, `Disabled ${itemID} is not hidden`);
      }
    } else {
      ok(!item.hidden, `${itemID} is not hidden`);
      ok(!item.disabled, `${itemID} is not disabled`);
    }
  }

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function sidebar_image_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();
  // Test sidebar is opened on install
  await extension.awaitMessage("sidebar");

  let contentAreaContextMenu = await openContextMenuInSidebar("#testimg");

  let item = contentAreaContextMenu.querySelector("#context-viewimageinfo");
  ok(!item.hidden);
  ok(!item.disabled);

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function cleanup() {
  // This is set on initial sidebar install.
  Services.prefs.clearUserPref("extensions.sidebar-button.shown");
});
