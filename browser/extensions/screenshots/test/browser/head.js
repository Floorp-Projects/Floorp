/* global CustomizableUI, info, Services */

// Currently Screenshots is disabled in tests.  We want these tests to work under
// either case that Screenshots is disabled or enabled on startup of the browser,
// and that at the end we're reset to the correct state.
let enabledOnStartup = false;

// ScreenshotsEnabled/Disabled promises return true if it was already
// Enabled/Disabled, and false if it need to Enable/Disable.
function promiseScreenshotsEnabled() {
  if (!Services.prefs.getBoolPref("extensions.screenshots.system-disabled", false)) {
    info("Screenshots was already enabled, assuming enabled by default for tests");
    enabledOnStartup = true;
    return Promise.resolve(true);
  }
  info("Screenshots is not enabled");
  return new Promise((resolve, reject) => {
    let listener = {
      onWidgetAfterCreation(widgetid) {
        if (widgetid == "screenshots_mozilla_org-browser-action") {
          info("screenshots_mozilla_org-browser-action button created");
          CustomizableUI.removeListener(listener);
          resolve(false);
        }
      }
    }
    CustomizableUI.addListener(listener);
    info("Set Screenshots disabled pref to false.");
    Services.prefs.setBoolPref("extensions.screenshots.system-disabled", false);
  });
}

function promiseScreenshotsDisabled() {
  if (Services.prefs.getBoolPref("extensions.screenshots.system-disabled", false)) {
    info("Screenshots already disabled");
    return Promise.resolve(true);
  }
  return new Promise((resolve, reject) => {
    let listener = {
      onWidgetDestroyed(widgetid) {
        if (widgetid == "screenshots_mozilla_org-browser-action") {
          CustomizableUI.removeListener(listener);
          info("screenshots_mozilla_org-browser-action destroyed");
          resolve(false);
        }
      }
    }
    CustomizableUI.addListener(listener);
    info("Set Screenshots disabled pref to true.");
    Services.prefs.setBoolPref("extensions.screenshots.system-disabled", true);
  });
}

function promiseScreenshotsReset() { // eslint-disable-line no-unused-vars
  if (enabledOnStartup) {
    info("Reset is enabling Screenshots addon");
    return promiseScreenshotsEnabled();
  }
  info("Reset is disabling Screenshots addon");
  return promiseScreenshotsDisabled();
}
