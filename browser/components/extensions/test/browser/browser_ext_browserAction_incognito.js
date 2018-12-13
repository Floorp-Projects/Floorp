/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testIncognito(incognitoOverride) {
  let privateAllowed = incognitoOverride != "not_allowed";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {},
    },
    incognitoOverride,
  });

  await extension.startup();
  await showBrowserAction(extension);

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await showBrowserAction(extension, privateWindow);

  let widgetId = makeWidgetId(extension.id) + "-browser-action";

  let node = window.document.getElementById(widgetId);
  ok(!!node, "popup exists in non-private window");

  node = privateWindow.document.getElementById(widgetId);
  if (privateAllowed) {
    ok(!!node, "popup exists in private window");
  } else {
    ok(!node, "popup does not exist in private window");
  }

  await BrowserTestUtils.closeWindow(privateWindow);
  await extension.unload();
}

add_task(async function test_browserAction_not_allowed() {
  await testIncognito("not_allowed");
});

add_task(async function test_browserAction_allowed() {
  await testIncognito();
});
