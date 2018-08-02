ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

this.FirefoxMonitor = {
  kEnabledPref: "extensions.fxmonitor.enabled",
  extension: null,
  init(aExtension) {
    this.extension = aExtension;

    if (!Preferences.get(this.kEnabledPref, false)) {
      return;
    }
  },
};
