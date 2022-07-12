/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
});

const kLushSoftID = "lush-soft-colorway@mozilla.org";
const kLushBoldID = "lush-bold-colorway@mozilla.org";
const kRetainedThemesPref = "browser.theme.retainedExpiredThemes";

add_task(async function retainExpiredActiveTheme() {
  let today = new Date().toISOString().split("T")[0];
  let tomorrow = new Date();
  tomorrow.setDate(tomorrow.getDate() + 1);
  tomorrow = tomorrow.toISOString().split("T")[0];
  let config = new Map([
    [
      kLushSoftID,
      {
        version: "1.1",
        path: "resource://builtin-themes/colorways/2021lush/soft/",
        expiry: tomorrow,
      },
    ],
    [
      kLushBoldID,
      {
        version: "1.1",
        path: "resource://builtin-themes/colorways/2021lush/bold/",
        expiry: tomorrow,
      },
    ],
  ]);
  const oldBuiltInThemeMap = BuiltInThemes.builtInThemeMap;
  BuiltInThemes.builtInThemeMap = config;
  Assert.equal(
    Services.prefs.getStringPref(kRetainedThemesPref, "[]"),
    "[]",
    "There are no retained themes."
  );

  AddonTestUtils.initMochitest(this);
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(kRetainedThemesPref);
    BuiltInThemes.builtInThemeMap = oldBuiltInThemeMap;
    await BuiltInThemes.ensureBuiltInThemes();
  });

  // Install our test themes and enable Lush (Soft).
  await BuiltInThemes.ensureBuiltInThemes();
  let lushSoft = await AddonManager.getAddonByID(kLushSoftID);
  let lushBold = await AddonManager.getAddonByID(kLushBoldID);
  await lushSoft.enable();
  Assert.ok(
    lushSoft && lushSoft.isActive,
    "Sanity check: Lush Soft is the active theme."
  );
  Assert.ok(
    lushBold && !lushBold.isActive,
    "Lush Bold is installed but inactive."
  );

  // Now, change the expiry dates on the themes to simulate the expiry date
  // passing.
  BuiltInThemes.builtInThemeMap.forEach(
    themeInfo => (themeInfo.expiry = today)
  );
  // Normally, ensureBuiltInThemes uninstalls expired themes. We
  // expect it will not uninstall Lush (Soft) since it is the active theme.
  await BuiltInThemes.ensureBuiltInThemes();
  lushSoft = await AddonManager.getAddonByID(kLushSoftID);
  lushBold = await AddonManager.getAddonByID(kLushBoldID);
  Assert.ok(
    lushSoft && lushSoft.isActive,
    "Lush Soft is still the active theme."
  );
  Assert.ok(!lushBold, "Lush Bold has been uninstalled.");
  Assert.equal(
    Services.prefs.getStringPref(kRetainedThemesPref, "[]"),
    JSON.stringify([kLushSoftID]),
    "Lush Soft is set as a retained theme."
  );

  // Disable Lush (Soft) and re-run ensureBuiltInThemes. We're checking that
  // Lush Soft is not uninstalled despite being inactive and expired, since it
  // is a retained theme.
  await lushSoft.disable();
  await BuiltInThemes.ensureBuiltInThemes();
  lushSoft = await AddonManager.getAddonByID(kLushSoftID);
  Assert.ok(
    lushSoft && !lushSoft.isActive,
    "Lush Soft is installed but inactive."
  );

  await lushSoft.uninstall();
});
