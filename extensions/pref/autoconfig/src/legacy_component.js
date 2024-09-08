//This config file is made by xiaoxiaoflood. Thanks. Pelase read about:license.

if (
  Services.prefs.getBoolPref(
    "toolkit.legacyUserProfileCustomizations.script",
    false
  )
) {
  lockPref("xpinstall.signatures.required", false);
  lockPref("extensions.install_origins.enabled", false);

  try {
    Services.scriptloader.loadSubScript(
      "chrome://userchromejs/content/BootstrapLoader.js"
    );
  } catch (ex) {}

  try {
    ChromeUtils.import("chrome://userchromejs/content/userChrome.jsm");
  } catch (ex) {}
}
