/* globals PageActions */

// Currently Screenshots is disabled in tests.  We want these tests to work under
// either case that Screenshots is disabled or enabled on startup of the browser,
// and that at the end we're reset to the correct state.
let enabledOnStartup = false;

// ScreenshotsEnabled/Disabled promises return true if it was already
// Enabled/Disabled, and false if it need to Enable/Disable.
function promiseScreenshotsEnabled() {
  if (!Services.prefs.getBoolPref("extensions.screenshots.disabled", false)) {
    info("Screenshots was already enabled, assuming enabled by default for tests");
    enabledOnStartup = true;
    return Promise.resolve(true);
  }
  info("Screenshots is not enabled");
  return new Promise((resolve, reject) => {
    const interval = setInterval(() => {
      const action = PageActions.actionForID("screenshots_mozilla_org");
      if (action) {
        info("screenshots page action created");
        clearInterval(interval);
        resolve(false);
      }
    }, 100);
    info("Set Screenshots disabled pref to false.");
    Services.prefs.setBoolPref("extensions.screenshots.disabled", false);
  });
}

function promiseScreenshotsDisabled() {
  if (Services.prefs.getBoolPref("extensions.screenshots.disabled", false)) {
    info("Screenshots already disabled");
    return Promise.resolve(true);
  }
  return new Promise((resolve, reject) => {
    const interval = setInterval(() => {
      const action = PageActions.actionForID("screenshots");
      if (!action) {
        info("screenshots page action removed");
        clearInterval(interval);
        resolve(false);
      }
    }, 100);
    info("Set Screenshots disabled pref to true.");
    Services.prefs.setBoolPref("extensions.screenshots.disabled", true);
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
