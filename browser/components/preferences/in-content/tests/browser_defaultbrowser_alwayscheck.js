"use strict";

const CHECK_DEFAULT_INITIAL = Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser");

add_task(function* clicking_make_default_checks_alwaysCheck_checkbox() {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  yield test_with_mock_shellservice({isDefault: false}, function*() {
    let setDefaultPane = content.document.getElementById("setDefaultPane");
    is(setDefaultPane.selectedIndex, "0",
       "The 'make default' pane should be visible when not default");
    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    is(alwaysCheck.checked, false, "Always Check is unchecked by default");
    is(Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"), false,
       "alwaysCheck pref should be false by default in test runs");

    let setDefaultButton = content.document.getElementById("setDefaultButton");
    setDefaultButton.click();
    content.window.gMainPane.updateSetDefaultBrowser();

    yield ContentTaskUtils.waitForCondition(() => alwaysCheck.checked,
      "'Always Check' checkbox should get checked after clicking the 'Set Default' button");

    is(alwaysCheck.checked, true,
       "Clicking 'Make Default' checks the 'Always Check' checkbox");
    is(Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"), true,
       "Checking the checkbox should set the pref to true");
    is(alwaysCheck.disabled, true,
       "'Always Check' checkbox is locked with default browser and alwaysCheck=true");
    is(setDefaultPane.selectedIndex, "1",
       "The 'make default' pane should not be visible when default");
    is(Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"), true,
       "checkDefaultBrowser pref is now enabled");
  });

  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("browser.shell.checkDefaultBrowser");
});

add_task(function* clicking_make_default_checks_alwaysCheck_checkbox() {
  Services.prefs.lockPref("browser.shell.checkDefaultBrowser");
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  yield test_with_mock_shellservice({isDefault: false}, function*() {
    let setDefaultPane = content.document.getElementById("setDefaultPane");
    is(setDefaultPane.selectedIndex, "0",
       "The 'make default' pane should be visible when not default");
    let alwaysCheck = content.document.getElementById("alwaysCheckDefault");
    is(alwaysCheck.disabled, true, "Always Check is disabled when locked");
    is(alwaysCheck.checked, true,
       "Always Check is checked because defaultPref is true and pref is locked");
    is(Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"), true,
       "alwaysCheck pref should ship with 'true' by default");

    let setDefaultButton = content.document.getElementById("setDefaultButton");
    setDefaultButton.click();
    content.window.gMainPane.updateSetDefaultBrowser();

    yield ContentTaskUtils.waitForCondition(() => setDefaultPane.selectedIndex == "1",
      "Browser is now default");

    is(alwaysCheck.checked, true,
       "'Always Check' is still checked because it's locked");
    is(alwaysCheck.disabled, true,
       "'Always Check is disabled because it's locked");
    is(Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"), true,
       "The pref is locked and so doesn't get changed");
  });

  Services.prefs.unlockPref("browser.shell.checkDefaultBrowser");
  gBrowser.removeCurrentTab();
});

registerCleanupFunction(function() {
  Services.prefs.unlockPref("browser.shell.checkDefaultBrowser");
  Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", CHECK_DEFAULT_INITIAL);
});

function* test_with_mock_shellservice(options, testFn) {
  yield ContentTask.spawn(gBrowser.selectedBrowser, options, function*(options) {
    let doc = content.document;
    let win = doc.defaultView;
    win.oldShellService = win.getShellService();
    let mockShellService = {
      _isDefault: false,
      isDefaultBrowser() {
        return this._isDefault;
      },
      setDefaultBrowser() {
        this._isDefault = true;
      },
    };
    win.getShellService = function() {
      return mockShellService;
    }
    mockShellService._isDefault = options.isDefault;
    win.gMainPane.updateSetDefaultBrowser();
  });

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, testFn);

  Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", CHECK_DEFAULT_INITIAL);
}
