/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWindowGetAll() {
  let raisedWin = Services.ww.openWindow(
    null, Services.prefs.getCharPref("browser.chromeURL"), "_blank",
    "chrome,dialog=no,all,alwaysRaised", null);

  await TestUtils.topicObserved("browser-delayed-startup-finished",
                                subject => subject == raisedWin);

  let extension = ExtensionTestUtils.loadExtension({
    background: async function() {
      let wins = await browser.windows.getAll();
      browser.test.assertEq(2, wins.length, "Expect two windows");

      browser.test.assertEq(false, wins[0].alwaysOnTop,
                            "Expect first window not to be always on top");
      browser.test.assertEq(true, wins[1].alwaysOnTop,
                            "Expect first window to be always on top");

      let win = await browser.windows.create({url: "http://example.com", type: "popup"});

      wins = await browser.windows.getAll();
      browser.test.assertEq(3, wins.length, "Expect three windows");

      wins = await browser.windows.getAll({windowTypes: ["popup"]});
      browser.test.assertEq(1, wins.length, "Expect one window");
      browser.test.assertEq("popup", wins[0].type,
                            "Expect type to be popup");

      wins = await browser.windows.getAll({windowTypes: ["normal"]});
      browser.test.assertEq(2, wins.length, "Expect two windows");
      browser.test.assertEq("normal", wins[0].type,
                            "Expect type to be normal");
      browser.test.assertEq("normal", wins[1].type,
                            "Expect type to be normal");

      wins = await browser.windows.getAll({windowTypes: ["popup", "normal"]});
      browser.test.assertEq(3, wins.length, "Expect three windows");

      await browser.windows.remove(win.id);

      browser.test.notifyPass("getAll");
    },
  });

  await extension.startup();
  await extension.awaitFinish("getAll");
  await extension.unload();

  await BrowserTestUtils.closeWindow(raisedWin);
});

add_task(async function testWindowTitle() {
  const PREFACE1 = "My prefix1 - ";
  const PREFACE2 = "My prefix2 - ";
  const START_URL = "http://example.com/browser/browser/components/extensions/test/browser/file_dummy.html";
  const START_TITLE = "Dummy test page";
  const NEW_URL = "http://example.com/browser/browser/components/extensions/test/browser/file_title.html";
  const NEW_TITLE = "Different title test page";

  async function background() {
    browser.test.onMessage.addListener(async (msg, options, windowId, expected) => {
      if (msg === "create") {
        let win = await browser.windows.create(options);
        browser.test.sendMessage("created", win);
      }
      if (msg === "update") {
        let win = await browser.windows.get(windowId);
        browser.test.assertTrue(win.title.startsWith(expected.before.preface),
                              "Window has the expected title preface before update.");
        browser.test.assertTrue(win.title.includes(expected.before.text),
                              "Window has the expected title text before update.");
        win = await browser.windows.update(windowId, options);
        browser.test.assertTrue(win.title.startsWith(expected.after.preface),
                              "Window has the expected title preface after update.");
        browser.test.assertTrue(win.title.includes(expected.after.text),
                              "Window has the expected title text after update.");
        browser.test.sendMessage("updated", win);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["tabs"],
    },
  });

  await extension.startup();
  let {Management: {global: {windowTracker}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  async function createApiWin(options) {
    let promiseLoaded = BrowserTestUtils.waitForNewWindow(START_URL);
    extension.sendMessage("create", options);
    let apiWin = await extension.awaitMessage("created");
    let realWin = windowTracker.getWindow(apiWin.id);
    await promiseLoaded;
    let expectedPreface = options.titlePreface ? options.titlePreface : "";
    ok(realWin.document.title.startsWith(expectedPreface),
       "Created window has the expected title preface.");
    ok(realWin.document.title.includes(START_TITLE),
       "Created window has the expected title text.");
    return apiWin;
  }

  async function updateWindow(options, apiWin, expected) {
    extension.sendMessage("update", options, apiWin.id, expected);
    await extension.awaitMessage("updated");
    let realWin = windowTracker.getWindow(apiWin.id);
    ok(realWin.document.title.startsWith(expected.after.preface),
       "Updated window has the expected title preface.");
    ok(realWin.document.title.includes(expected.after.text),
       "Updated window has the expected title text.");
    await BrowserTestUtils.closeWindow(realWin);
  }

  // Create a window without a preface.
  let apiWin = await createApiWin({url: START_URL});

  // Add a titlePreface to the window.
  let expected = {
    before: {
      preface: "",
      text: START_TITLE,
    },
    after: {
      preface: PREFACE1,
      text: START_TITLE,
    },
  };
  await updateWindow({titlePreface: PREFACE1}, apiWin, expected);

  // Create a window with a preface.
  apiWin = await createApiWin({url: START_URL, titlePreface: PREFACE1});

  // Navigate to a different url and check that title is reflected.
  let realWin = windowTracker.getWindow(apiWin.id);
  let promiseLoaded = BrowserTestUtils.browserLoaded(realWin.gBrowser.selectedBrowser);
  await BrowserTestUtils.loadURI(realWin.gBrowser.selectedBrowser, NEW_URL);
  await promiseLoaded;
  ok(realWin.document.title.startsWith(PREFACE1),
     "Updated window has the expected title preface.");
  ok(realWin.document.title.includes(NEW_TITLE),
     "Updated window has the expected title text.");

  // Update the titlePreface of the window.
  expected = {
    before: {
      preface: PREFACE1,
      text: NEW_TITLE,
    },
    after: {
      preface: PREFACE2,
      text: NEW_TITLE,
    },
  };
  await updateWindow({titlePreface: PREFACE2}, apiWin, expected);

  // Create a window with a preface.
  apiWin = await createApiWin({url: START_URL, titlePreface: PREFACE1});
  realWin = windowTracker.getWindow(apiWin.id);

  // Update the titlePreface of the window with an empty string.
  expected = {
    before: {
      preface: PREFACE1,
      text: START_TITLE,
    },
    after: {
      preface: "",
      text: START_TITLE,
    },
  };
  await updateWindow({titlePreface: ""}, apiWin, expected);
  ok(!realWin.document.title.startsWith(expected.before.preface), "Updated window has the expected empty title preface.");

  // Create a window with a preface.
  apiWin = await createApiWin({url: START_URL, titlePreface: PREFACE1});
  realWin = windowTracker.getWindow(apiWin.id);

  // Update the window without a titlePreface.
  expected = {
    before: {
      preface: PREFACE1,
      text: START_TITLE,
    },
    after: {
      preface: PREFACE1,
      text: START_TITLE,
    },
  };
  await updateWindow({}, apiWin, expected);

  await extension.unload();
});

// Test that the window title is only available with the correct tab
// permissions.
add_task(async function testWindowTitlePermissions() {
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      function awaitMessage(name) {
        return new Promise(resolve => {
          browser.test.onMessage.addListener(function listener(...msg) {
            if (msg[0] === name) {
              browser.test.onMessage.removeListener(listener);
              resolve(msg[1]);
            }
          });
        });
      }

      let window = await browser.windows.getCurrent();

      browser.test.assertEq(undefined, window.title,
                            "Window title should be null without tab permission");

      browser.test.sendMessage("grant-activeTab");
      let expectedTitle = await awaitMessage("title");

      window = await browser.windows.getCurrent();
      browser.test.assertEq(expectedTitle, window.title,
                            "Window should have the expected title with tab permission granted");

      await browser.test.notifyPass("window-title-permissions");
    },
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });

  await extension.startup();

  await extension.awaitMessage("grant-activeTab");
  await clickBrowserAction(extension);
  extension.sendMessage("title", document.title);

  await extension.awaitFinish("window-title-permissions");

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testInvalidWindowId() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.test.assertRejects(
        // Assuming that this windowId does not exist.
        browser.windows.get(123456789),
        /Invalid window/,
        "Should receive invalid window");
      browser.test.notifyPass("windows.get.invalid");
    },
  });

  await extension.startup();
  await extension.awaitFinish("windows.get.invalid");
  await extension.unload();
});
