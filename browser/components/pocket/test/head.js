// Currently Pocket is disabled in tests.  We want these tests to work under
// either case that Pocket is disabled or enabled on startup of the browser,
// and that at the end we're reset to the correct state.
let enabledOnStartup = false;

ChromeUtils.defineModuleGetter(
  this,
  "pktApi",
  "chrome://pocket/content/pktApi.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// PocketEnabled/Disabled promises return true if it was already
// Enabled/Disabled, and false if it need to Enable/Disable.
function promisePocketEnabled() {
  if (
    Services.prefs.getPrefType("extensions.pocket.enabled") !=
      Services.prefs.PREF_INVALID &&
    Services.prefs.getBoolPref("extensions.pocket.enabled")
  ) {
    info("pocket was already enabled, assuming enabled by default for tests");
    enabledOnStartup = true;
    return Promise.resolve(true);
  }
  info("pocket is not enabled");
  Services.prefs.setBoolPref("extensions.pocket.enabled", true);
  return BrowserTestUtils.waitForCondition(() => {
    return !!CustomizableUI.getWidget("save-to-pocket-button");
  });
}

function promisePocketDisabled() {
  if (
    Services.prefs.getPrefType("extensions.pocket.enabled") ==
      Services.prefs.PREF_INVALID ||
    !Services.prefs.getBoolPref("extensions.pocket.enabled")
  ) {
    info("pocket-button already disabled");
    return Promise.resolve(true);
  }
  info("reset pocket enabled pref");
  // testing/profiles/common/user.js uses user_pref to disable pocket, set
  // back to false.
  Services.prefs.setBoolPref("extensions.pocket.enabled", false);
  return BrowserTestUtils.waitForCondition(() => {
    return !CustomizableUI.getWidget("save-to-pocket-button");
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

function checkElements(expectPresent, l, win = window) {
  for (let id of l) {
    let el =
      win.document.getElementById(id) ||
      win.gNavToolbox.palette.querySelector("#" + id);
    is(
      !!el && !el.hidden,
      expectPresent,
      "element " + id + (expectPresent ? " is" : " is not") + " present"
    );
  }
}

function checkElementsShown(expectPresent, l, win = window) {
  for (let id of l) {
    let el =
      win.document.getElementById(id) ||
      win.gNavToolbox.palette.querySelector("#" + id);
    let elShown = !!el && window.getComputedStyle(el).display != "none";
    is(
      elShown,
      expectPresent,
      "element " + id + (expectPresent ? " is" : " is not") + " present"
    );
  }
}
