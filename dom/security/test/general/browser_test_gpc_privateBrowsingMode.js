"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kTestURI = kTestPath + "file_empty.html";

add_task(async function test_privateModeGPCEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.globalprivacycontrol.enabled", false],
      ["privacy.globalprivacycontrol.pbmode.enabled", true],
      ["privacy.globalprivacycontrol.functionality.enabled", true],
    ],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, kTestURI);
  let browser = win.gBrowser.getBrowserForTab(tab);
  let result = await SpecialPowers.spawn(browser, [], async function () {
    return content.window
      .fetch("file_gpc_server.sjs")
      .then(response => response.text())
      .then(response => {
        is(response, "true", "GPC header provided");
        is(
          content.window.navigator.globalPrivacyControl,
          true,
          "GPC on navigator"
        );
        // Bug 1320796: Service workers are not enabled in PB Mode
        return true;
      });
  });
  ok(result, "Promise chain resolves in content process");
  win.close();
});

add_task(async function test_privateModeGPCDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.globalprivacycontrol.enabled", false],
      ["privacy.globalprivacycontrol.pbmode.enabled", false],
      ["privacy.globalprivacycontrol.functionality.enabled", true],
    ],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, kTestURI);
  let browser = win.gBrowser.getBrowserForTab(tab);
  let result = await SpecialPowers.spawn(browser, [], async function () {
    return content.window
      .fetch("file_gpc_server.sjs")
      .then(response => response.text())
      .then(response => {
        isnot(response, "true", "GPC header provided");
        isnot(
          content.window.navigator.globalPrivacyControl,
          true,
          "GPC on navigator"
        );
        // Bug 1320796: Service workers are not enabled in PB Mode
        return true;
      });
  });
  ok(result, "Promise chain resolves in content process");
  win.close();
});
