"use strict";

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

loadTestSubscript("head_unified_extensions.js");

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

async function makeExtension({ id, permissions, host_permissions, granted }) {
  info(
    `Loading extension ` +
      JSON.stringify({ id, permissions, host_permissions, granted })
  );

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      browser_specific_settings: { gecko: { id } },
      permissions,
      host_permissions,
      action: {
        default_popup: "popup.html",
      },
    },

    useAddonManager: "temporary",

    background() {
      browser.permissions.onAdded.addListener(({ origins }) => {
        browser.test.sendMessage("granted", origins.join());
      });
      browser.permissions.onRemoved.addListener(({ origins }) => {
        browser.test.sendMessage("revoked", origins.join());
      });
    },

    files: {
      "popup.html": `<!DOCTYPE html><meta charset=utf-8>Test Popup`,
    },
  });

  if (granted) {
    info("Granting initial permissions.");
    await ExtensionPermissions.add(id, { permissions: [], origins: granted });
  }

  await ext.startup();
  return ext;
}

async function testOriginControls(
  extension,
  { win, contextMenuId },
  { items, selected, click, granted, revoked }
) {
  info(
    `Testing ${extension.id} on ${gBrowser.currentURI.spec} with contextMenuId=${contextMenuId}.`
  );

  let menu;
  let manageExtensionClassName;

  switch (contextMenuId) {
    case "toolbar-context-menu":
      let target = `#${CSS.escape(makeWidgetId(extension.id))}-browser-action`;
      menu = await openChromeContextMenu(contextMenuId, target);
      manageExtensionClassName = "customize-context-manageExtension";
      break;

    case "unified-extensions-context-menu":
      await openExtensionsPanel(win);
      menu = await openUnifiedExtensionsContextMenu(win, extension.id);
      manageExtensionClassName =
        "unified-extensions-context-menu-manage-extension";
      break;

    default:
      throw new Error(`unexpected context menu "${contextMenuId}"`);
  }

  let doc = menu.ownerDocument;

  info("Check expected menu items.");
  for (let i = 0; i < items.length; i++) {
    let l10n = doc.l10n.getAttributes(menu.children[i]);
    Assert.deepEqual(l10n, items[i], `Menu item ${i} has correct l10n attrs.`);

    let checked = menu.children[i].getAttribute("checked") === "true";
    is(i === selected, checked, `Expected checked value for item ${i}.`);
  }

  is(menu.children[items.length].nodeName, "menuseparator", "Found separator.");
  is(
    menu.children[items.length + 1].className,
    manageExtensionClassName,
    "All items accounted for."
  );

  let itemToClick;
  if (click) {
    itemToClick = menu.children[click];
  }
  await closeChromeContextMenu(contextMenuId, itemToClick, win);

  if (contextMenuId === "unified-extensions-context-menu") {
    await closeExtensionsPanel(win);
  }

  if (granted) {
    info("Waiting for the permissions.onAdded event.");
    let host = await extension.awaitMessage("granted");
    is(host, granted.join(), "Expected host permission granted.");
  }
  if (revoked) {
    info("Waiting for the permissions.onRemoved event.");
    let host = await extension.awaitMessage("revoked");
    is(host, revoked.join(), "Expected host permission revoked.");
  }
}

const originControlsInContextMenu = async options => {
  // Has no permissions.
  let ext1 = await makeExtension({ id: "ext1@test" });

  // Has activeTab and (ungranted) example.com permissions.
  let ext2 = await makeExtension({
    id: "ext2@test",
    permissions: ["activeTab"],
    host_permissions: ["*://example.com/*"],
  });

  // Has ungranted <all_urls>, and granted example.com.
  let ext3 = await makeExtension({
    id: "ext3@test",
    host_permissions: ["<all_urls>"],
    granted: ["*://example.com/*"],
  });

  // Has granted <all_urls>.
  let ext4 = await makeExtension({
    id: "ext4@test",
    host_permissions: ["<all_urls>"],
    granted: ["<all_urls>"],
  });

  let extensions = [ext1, ext2, ext3, ext4];

  const NO_ACCESS = { id: "origin-controls-no-access", args: null };
  const ACCESS_OPTIONS = { id: "origin-controls-options", args: null };
  const ALL_SITES = { id: "origin-controls-option-all-domains", args: null };
  const WHEN_CLICKED = {
    id: "origin-controls-option-when-clicked",
    args: null,
  };

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await testOriginControls(ext1, options, { items: [NO_ACCESS] });
    await testOriginControls(ext2, options, { items: [NO_ACCESS] });
    await testOriginControls(ext3, options, { items: [NO_ACCESS] });
    await testOriginControls(ext4, options, { items: [NO_ACCESS] });
  });

  await BrowserTestUtils.withNewTab("http://mochi.test:8888/", async () => {
    const ALWAYS_ON = {
      id: "origin-controls-option-always-on",
      args: { domain: "mochi.test" },
    };

    await testOriginControls(ext1, options, { items: [NO_ACCESS] });

    // Has activeTab.
    await testOriginControls(ext2, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED],
      selected: 1,
    });

    // Could access mochi.test when clicked.
    await testOriginControls(ext3, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 1,
    });

    // Has <all_urls> granted.
    await testOriginControls(ext4, options, {
      items: [ACCESS_OPTIONS, ALL_SITES],
      selected: 1,
    });
  });

  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    const ALWAYS_ON = {
      id: "origin-controls-option-always-on",
      args: { domain: "example.com" },
    };

    await testOriginControls(ext1, options, { items: [NO_ACCESS] });

    // Click alraedy selected options, expect no permission changes.
    await testOriginControls(ext2, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 1,
      click: 1,
    });
    await testOriginControls(ext3, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 2,
      click: 2,
    });
    await testOriginControls(ext4, options, {
      items: [ACCESS_OPTIONS, ALL_SITES],
      selected: 1,
      click: 1,
    });

    // Click the other option, expect example.com permission granted/revoked.
    await testOriginControls(ext2, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 1,
      click: 2,
      granted: ["*://example.com/*"],
    });
    await testOriginControls(ext3, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 2,
      click: 1,
      revoked: ["*://example.com/*"],
    });

    // Other option is now selected.
    await testOriginControls(ext2, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 2,
    });
    await testOriginControls(ext3, options, {
      items: [ACCESS_OPTIONS, WHEN_CLICKED, ALWAYS_ON],
      selected: 1,
    });
  });

  await Promise.all(extensions.map(e => e.unload()));
};

add_task(async function originControls_in_browserAction_contextMenu() {
  await originControlsInContextMenu({
    win: window,
    contextMenuId: "toolbar-context-menu",
  });
});

add_task(async function originControls_in_unifiedExtensions_contextMenu() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.enabled", true]],
  });

  const win = await promiseEnableUnifiedExtensions();

  await originControlsInContextMenu({
    win,
    contextMenuId: "unified-extensions-context-menu",
  });

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
