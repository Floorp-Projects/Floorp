"use strict";

// Make sure that we won't trigger events for a private window.
add_task(async function test_no_show_hide_for_private_window() {
  function background() {
    let events = [];
    browser.menus.onShown.addListener(data => events.push(data));
    browser.menus.onHidden.addListener(() => events.push("onHidden"));
    browser.test.onMessage.addListener(async (name, data) => {
      if (name == "check-events") {
        browser.test.sendMessage("events", events);
        events = [];
      }
      if (name == "create-menu") {
        let id = await new Promise(resolve => {
          let mid = browser.menus.create(data, () => resolve(mid));
        });
        browser.test.sendMessage("menu-id", id);
      }
    });
  }

  let pb_extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: {
        gecko: { id: "@private-allowed" },
      },
      permissions: ["menus", "tabs"],
    },
    incognitoOverride: "spanning",
  });

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: {
        gecko: { id: "@not-allowed" },
      },
      permissions: ["menus", "tabs"],
    },
  });

  async function testEvents(ext, expected) {
    ext.sendMessage("check-events");
    let events = await ext.awaitMessage("events");
    Assert.deepEqual(
      expected,
      events,
      `expected events received for ${ext.id}.`
    );
  }

  await pb_extension.startup();
  await extension.startup();

  extension.sendMessage("create-menu", {
    title: "not_allowed",
    contexts: ["all", "tools_menu"],
  });
  let id1 = await extension.awaitMessage("menu-id");
  let extMenuId = `${makeWidgetId(extension.id)}-menuitem-${id1}`;
  pb_extension.sendMessage("create-menu", {
    title: "spanning_allowed",
    contexts: ["all", "tools_menu"],
  });
  let id2 = await pb_extension.awaitMessage("menu-id");
  let pb_extMenuId = `${makeWidgetId(pb_extension.id)}-menuitem-${id2}`;

  // Expected menu events
  let baseShownEvent = {
    contexts: ["page", "all"],
    viewType: "tab",
    frameId: 0,
    editable: false,
  };
  let publicShown = { menuIds: [id1], ...baseShownEvent };
  let privateShown = { menuIds: [id2], ...baseShownEvent };

  baseShownEvent = {
    contexts: ["tools_menu"],
    viewType: undefined,
    editable: false,
  };
  let toolsShown = { menuIds: [id1], ...baseShownEvent };
  let privateToolsShown = { menuIds: [id2], ...baseShownEvent };

  // Run tests in non-private window

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:robots"
  );

  // Open and close a menu on the public window.
  await openContextMenu("body");

  // We naturally expect both extensions here.
  ok(document.getElementById(extMenuId), `menu exists ${extMenuId}`);
  ok(document.getElementById(pb_extMenuId), `menu exists ${pb_extMenuId}`);
  await closeContextMenu();

  await testEvents(extension, [publicShown, "onHidden"]);
  await testEvents(pb_extension, [privateShown, "onHidden"]);

  await openToolsMenu();
  ok(document.getElementById(extMenuId), `menu exists ${extMenuId}`);
  ok(document.getElementById(pb_extMenuId), `menu exists ${pb_extMenuId}`);
  await closeToolsMenu();

  await testEvents(extension, [toolsShown, "onHidden"]);
  await testEvents(pb_extension, [privateToolsShown, "onHidden"]);

  // Run tests on private window

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open and close a menu on the private window.
  let menu = await openContextMenu("body", privateWindow);
  // We should not see the "not_allowed" extension here.
  ok(
    !privateWindow.document.getElementById(extMenuId),
    `menu does not exist ${extMenuId} in private window`
  );
  ok(
    privateWindow.document.getElementById(pb_extMenuId),
    `menu exists ${pb_extMenuId} in private window`
  );
  await closeContextMenu(menu);

  await testEvents(extension, []);
  await testEvents(pb_extension, [privateShown, "onHidden"]);

  await openToolsMenu(privateWindow);
  // We should not see the "not_allowed" extension here.
  ok(
    !privateWindow.document.getElementById(extMenuId),
    `menu does not exist ${extMenuId} in private window`
  );
  ok(
    privateWindow.document.getElementById(pb_extMenuId),
    `menu exists ${pb_extMenuId} in private window`
  );
  await closeToolsMenu(undefined, privateWindow);

  await testEvents(extension, []);
  await testEvents(pb_extension, [privateToolsShown, "onHidden"]);

  await extension.unload();
  await pb_extension.unload();
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(privateWindow);
});
