// Currently Pocket is disabled in tests.  We want these tests to work under
// either case that Pocket is disabled or enabled on startup of the browser,
// and that at the end we're reset to the correct state.
let enabledOnStartup = false;

// PocketEnabled/Disabled promises return true if it was already
// Enabled/Disabled, and false if it need to Enable/Disable.
function promisePocketEnabled() {
  if (Services.prefs.getPrefType("extensions.pocket.enabled") != Services.prefs.PREF_INVALID &&
      Services.prefs.getBoolPref("extensions.pocket.enabled")) {
    info( "pocket was already enabled, assuming enabled by default for tests");
    enabledOnStartup = true;
    return Promise.resolve(true);
  }
  info( "pocket is not enabled");
  return new Promise((resolve, reject) => {
    let listener = {
      onWidgetAfterCreation(widgetid) {
        if (widgetid == "pocket-button") {
          info("pocket-button created");
          CustomizableUI.removeListener(listener);
          resolve(false);
        }
      }
    }
    CustomizableUI.addListener(listener);
    Services.prefs.setBoolPref("extensions.pocket.enabled", true);
  });
}

function promisePocketDisabled() {
  if (Services.prefs.getPrefType("extensions.pocket.enabled") == Services.prefs.PREF_INVALID ||
      !Services.prefs.getBoolPref("extensions.pocket.enabled")) {
    info("pocket-button already disabled");
    return Promise.resolve(true);
  }
  return new Promise((resolve, reject) => {
    let listener = {
      onWidgetDestroyed: function(widgetid) {
        if (widgetid == "pocket-button") {
          CustomizableUI.removeListener(listener);
          info( "pocket-button destroyed");
          // wait for a full unload of pocket
          BrowserTestUtils.waitForCondition(() => {
            return !window.hasOwnProperty("pktUI");
          }, "pocket properties removed from window").then(() => {
            resolve(false);
          })
        }
      }
    }
    CustomizableUI.addListener(listener);
    info("reset pocket enabled pref");
    // testing/profiles/prefs_general.js uses user_pref to disable pocket, set
    // back to false.
    Services.prefs.setBoolPref("extensions.pocket.enabled", false);
  });
}

function promisePocketReset() {
  if (enabledOnStartup) {
    info("reset is enabling pocket addon");
    return promisePocketEnabled();
  }
  info("reset is disabling pocket addon");
  return promisePocketDisabled();
}
