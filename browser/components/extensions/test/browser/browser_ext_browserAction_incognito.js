/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testIncognito(incognitoOverride) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_area: "navbar",
      },
    },
    incognitoOverride,
  });

  // We test three windows, the public window, a private window prior
  // to extension start, and one created after.  This tests that CUI
  // creates the widgets (or not) as it should.
  let p1 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await extension.startup();
  let p2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  let action = getBrowserActionWidget(extension);
  await showBrowserAction(extension);
  await showBrowserAction(extension, p1);
  await showBrowserAction(extension, p2);

  ok(!!action.forWindow(window).node, "popup exists in non-private window");

  for (let win of [p1, p2]) {
    let node = action.forWindow(win).node;
    if (incognitoOverride == "spanning") {
      ok(!!node, "popup exists in private window");
    } else {
      ok(!node, "popup does not exist in private window");
    }

    await BrowserTestUtils.closeWindow(win);
  }
  await extension.unload();
}

add_task(async function test_browserAction_not_allowed() {
  await testIncognito();
});

add_task(async function test_browserAction_allowed() {
  await testIncognito("spanning");
});
