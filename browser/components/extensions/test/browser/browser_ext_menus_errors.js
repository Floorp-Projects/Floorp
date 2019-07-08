/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test_create_error() {
  // lastError is the only means to communicate errors in the menus.create API,
  // so make sure that a warning is logged to the console if the error is not
  // checked.
  let waitForConsole = new Promise(resolve => {
    SimpleTest.waitForExplicitFinish();
    SimpleTest.monitorConsole(resolve, [
      // Callback exists, lastError is checked. Should not be logged.
      {
        message: /Unchecked lastError value: Error: ID already exists: some_id/,
        forbid: true,
      },
      // No callback, lastError not checked. Should be logged.
      {
        message: /Unchecked lastError value: Error: Could not find any MenuItem with id: noCb/,
      },
      // Callback exists, lastError not checked. Should be logged.
      {
        message: /Unchecked lastError value: Error: Could not find any MenuItem with id: cbIgnoreError/,
      },
    ]);
  });

  async function background() {
    // Note: browser.menus.create returns the menu ID instead of a promise, so
    // we have to use callbacks.
    await new Promise(resolve => {
      browser.menus.create({ id: "some_id", title: "menu item" }, () => {
        browser.test.assertEq(
          null,
          browser.runtime.lastError,
          "Expected no error"
        );
        resolve();
      });
    });

    // Callback exists, lastError is checked:
    await new Promise(resolve => {
      browser.menus.create({ id: "some_id", title: "menu item" }, () => {
        browser.test.assertEq(
          "ID already exists: some_id",
          browser.runtime.lastError.message,
          "Expected error"
        );
        resolve();
      });
    });

    // No callback, lastError not checked:
    browser.menus.create({ id: "noCb", parentId: "noCb", title: "menu item" });

    // Callback exists, lastError not checked:
    await new Promise(resolve => {
      browser.menus.create(
        { id: "cbIgnoreError", parentId: "cbIgnoreError", title: "menu item" },
        () => {
          resolve();
        }
      );
    });

    // Do another roundtrip with the menus API to ensure that any console
    // error messages from the previous call are flushed.
    await browser.menus.removeAll();

    browser.test.sendMessage("done");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["menus"] },
    background,
  });
  await extension.startup();
  await extension.awaitMessage("done");

  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_update_error() {
  async function background() {
    const id = browser.menus.create({ title: "menu item" });

    await browser.test.assertRejects(
      browser.menus.update(id, { parentId: "bogus" }),
      "Could not find any MenuItem with id: bogus",
      "menus.update with invalid parentMenuId should fail"
    );

    await browser.test.assertRejects(
      browser.menus.update(id, { parentId: id }),
      "MenuItem cannot be an ancestor (or self) of its new parent.",
      "menus.update cannot assign itself as the parent of a menu."
    );

    browser.test.sendMessage("done");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["menus"] },
    background,
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
