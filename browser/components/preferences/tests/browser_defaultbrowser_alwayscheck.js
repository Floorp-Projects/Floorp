"use strict";

const CHECK_DEFAULT_INITIAL = Services.prefs.getBoolPref(
  "browser.shell.checkDefaultBrowser"
);

add_task(async function clicking_make_default_checks_alwaysCheck_checkbox() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  await test_with_mock_shellservice({ isDefault: false }, async function () {
    let checkDefaultBrowserState = isDefault => {
      let isDefaultPane = content.document.getElementById("isDefaultPane");
      let isNotDefaultPane =
        content.document.getElementById("isNotDefaultPane");
      Assert.equal(
        ContentTaskUtils.isHidden(isDefaultPane),
        !isDefault,
        "The 'browser is default' pane should be hidden when browser is not default"
      );
      Assert.equal(
        ContentTaskUtils.isHidden(isNotDefaultPane),
        isDefault,
        "The 'make default' pane should be hidden when browser is default"
      );
    };

    checkDefaultBrowserState(false);

    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    Assert.ok(!alwaysCheck.checked, "Always Check is unchecked by default");
    Assert.ok(
      !Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "alwaysCheck pref should be false by default in test runs"
    );

    let setDefaultButton = content.document.getElementById("setDefaultButton");
    setDefaultButton.click();
    content.window.gMainPane.updateSetDefaultBrowser();

    await ContentTaskUtils.waitForCondition(
      () => alwaysCheck.checked,
      "'Always Check' checkbox should get checked after clicking the 'Set Default' button"
    );

    Assert.ok(
      alwaysCheck.checked,
      "Clicking 'Make Default' checks the 'Always Check' checkbox"
    );
    Assert.ok(
      Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "Checking the checkbox should set the pref to true"
    );
    Assert.ok(
      alwaysCheck.disabled,
      "'Always Check' checkbox is locked with default browser and alwaysCheck=true"
    );
    checkDefaultBrowserState(true);
    Assert.ok(
      Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "checkDefaultBrowser pref is now enabled"
    );
  });

  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("browser.shell.checkDefaultBrowser");
});

add_task(async function clicking_make_default_checks_alwaysCheck_checkbox() {
  Services.prefs.lockPref("browser.shell.checkDefaultBrowser");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  await test_with_mock_shellservice({ isDefault: false }, async function () {
    let isDefaultPane = content.document.getElementById("isDefaultPane");
    let isNotDefaultPane = content.document.getElementById("isNotDefaultPane");
    Assert.ok(
      ContentTaskUtils.isHidden(isDefaultPane),
      "The 'browser is default' pane should be hidden when not default"
    );
    Assert.ok(
      ContentTaskUtils.isVisible(isNotDefaultPane),
      "The 'make default' pane should be visible when not default"
    );

    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    Assert.ok(alwaysCheck.disabled, "Always Check is disabled when locked");
    Assert.ok(
      alwaysCheck.checked,
      "Always Check is checked because defaultPref is true and pref is locked"
    );
    Assert.ok(
      Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "alwaysCheck pref should ship with 'true' by default"
    );

    let setDefaultButton = content.document.getElementById("setDefaultButton");
    setDefaultButton.click();
    content.window.gMainPane.updateSetDefaultBrowser();

    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.isVisible(isDefaultPane),
      "Browser is now default"
    );

    Assert.ok(
      alwaysCheck.checked,
      "'Always Check' is still checked because it's locked"
    );
    Assert.ok(
      alwaysCheck.disabled,
      "'Always Check is disabled because it's locked"
    );
    Assert.ok(
      Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "The pref is locked and so doesn't get changed"
    );
  });

  Services.prefs.unlockPref("browser.shell.checkDefaultBrowser");
  gBrowser.removeCurrentTab();
});

add_task(async function make_default_disabled_until_prefs_are_loaded() {
  // Testcase with Firefox not set as the default browser
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await test_with_mock_shellservice({ isDefault: false }, async function () {
    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    Assert.ok(
      !alwaysCheck.disabled,
      "'Always Check' is enabled after default browser updated"
    );
  });
  gBrowser.removeCurrentTab();

  // Testcase with Firefox set as the default browser
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await test_with_mock_shellservice({ isDefault: true }, async function () {
    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    Assert.ok(
      alwaysCheck.disabled,
      "'Always Check' is still disabled after default browser updated"
    );
  });
  gBrowser.removeCurrentTab();
});

registerCleanupFunction(function () {
  Services.prefs.unlockPref("browser.shell.checkDefaultBrowser");
  Services.prefs.setBoolPref(
    "browser.shell.checkDefaultBrowser",
    CHECK_DEFAULT_INITIAL
  );
});

async function test_with_mock_shellservice(options, testFn) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [options],
    async function (contentOptions) {
      let doc = content.document;
      let win = doc.defaultView;
      win.oldShellService = win.getShellService();
      let mockShellService = {
        _isDefault: false,
        isDefaultBrowser() {
          return this._isDefault;
        },
        async setDefaultBrowser() {
          this._isDefault = true;
        },
      };
      win.getShellService = function () {
        return mockShellService;
      };
      mockShellService._isDefault = contentOptions.isDefault;
      win.gMainPane.updateSetDefaultBrowser();
    }
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], testFn);

  Services.prefs.setBoolPref(
    "browser.shell.checkDefaultBrowser",
    CHECK_DEFAULT_INITIAL
  );
}
