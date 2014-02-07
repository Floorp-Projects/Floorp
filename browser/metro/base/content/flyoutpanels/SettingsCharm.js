
/**
 * Manage the contents of the Windows 8 "Settings" charm.
 */
var SettingsCharm = {
  _entries: new Map(),
  _nextId: 0,

  /**
   * Add a new item to the "Settings" menu in the Windows 8 charms.
   * @param aEntry Object with a "label" property (string that will appear in the UI)
   *    and an "onselected" property (function to be called when the user chooses this entry)
   */
  addEntry: function addEntry(aEntry) {
    try {
      let id = Services.metro.addSettingsPanelEntry(aEntry.label);
      this._entries.set(id, aEntry);
    } catch (e) {
      // addSettingsPanelEntry does not work on non-Metro platforms
      Cu.reportError(e);
    }
  },

  init: function SettingsCharm_init() {
    Services.obs.addObserver(this, "metro-settings-entry-selected", false);

    // Options
    this.addEntry({
        label: Strings.browser.GetStringFromName("optionsCharm"),
        onselected: function() FlyoutPanelsUI.show('PrefsFlyoutPanel')
    });

    // Search providers
    this.addEntry({
        label: Strings.browser.GetStringFromName("searchCharm"),
        onselected: function() FlyoutPanelsUI.show('SearchFlyoutPanel')
    });

/*
 * Temporarily disabled until we can have sync prefs together with the
 * Desktop browser's sync prefs.
    // Sync
    this.addEntry({
        label: Strings.brand.GetStringFromName("syncBrandShortName"),
        onselected: function() FlyoutPanelsUI.show('SyncFlyoutPanel')
    });
*/

    // About
    this.addEntry({
        label: Strings.browser.GetStringFromName("aboutCharm1"),
        onselected: function() FlyoutPanelsUI.show('AboutFlyoutPanel')
    });

    // Help
    this.addEntry({
        label: Strings.browser.GetStringFromName("helpOnlineCharm"),
        onselected: function() {
          let url = Services.urlFormatter.formatURLPref("app.support.baseURL") +
            "firefox-help";
          BrowserUI.addAndShowTab(url, Browser.selectedTab);
        }
    });
  },

  observe: function SettingsCharm_observe(aSubject, aTopic, aData) {
    if (aTopic == "metro-settings-entry-selected") {
      let entry = this._entries.get(parseInt(aData, 10));
      if (entry)
        entry.onselected();
    }
  },

  uninit: function SettingsCharm_uninit() {
    Services.obs.removeObserver(this, "metro-settings-entry-selected");
  }
};
