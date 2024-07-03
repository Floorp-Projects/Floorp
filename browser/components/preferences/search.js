/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUIUtils: "resource:///modules/SearchUIUtils.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

const PREF_URLBAR_QUICKSUGGEST_BLOCKLIST =
  "browser.urlbar.quicksuggest.blockedDigests";
const PREF_URLBAR_WEATHER_USER_ENABLED = "browser.urlbar.suggest.weather";

Preferences.addAll([
  { id: "browser.search.suggest.enabled", type: "bool" },
  { id: "browser.urlbar.suggest.searches", type: "bool" },
  { id: "browser.search.suggest.enabled.private", type: "bool" },
  { id: "browser.search.widget.inNavBar", type: "bool" },
  { id: "browser.urlbar.showSearchSuggestionsFirst", type: "bool" },
  { id: "browser.urlbar.showSearchTerms.enabled", type: "bool" },
  { id: "browser.search.separatePrivateDefault", type: "bool" },
  { id: "browser.search.separatePrivateDefault.ui.enabled", type: "bool" },
  { id: "browser.urlbar.suggest.trending", type: "bool" },
  { id: "browser.urlbar.trending.featureGate", type: "bool" },
  { id: "browser.urlbar.trending.enabledLocales", type: "string" },
  { id: "browser.urlbar.recentsearches.featureGate", type: "bool" },
  { id: "browser.urlbar.suggest.recentsearches", type: "bool" },
]);

const ENGINE_FLAVOR = "text/x-moz-search-engine";
const SEARCH_TYPE = "default_search";
const SEARCH_KEY = "defaultSearch";

var gEngineView = null;

var gSearchPane = {
  _engineStore: null,
  _engineDropDown: null,
  _engineDropDownPrivate: null,

  init() {
    this._engineStore = new EngineStore();
    gEngineView = new EngineView(this._engineStore);

    this._engineDropDown = new DefaultEngineDropDown(
      "normal",
      this._engineStore
    );
    this._engineDropDownPrivate = new DefaultEngineDropDown(
      "private",
      this._engineStore
    );

    this._engineStore.init().catch(console.error);

    if (
      Services.policies &&
      !Services.policies.isAllowed("installSearchEngine")
    ) {
      document.getElementById("addEnginesBox").hidden = true;
    } else {
      let addEnginesLink = document.getElementById("addEngines");
      addEnginesLink.setAttribute("href", lazy.SearchUIUtils.searchEnginesURL);
    }

    window.addEventListener("command", this);

    Services.obs.addObserver(this, "browser-search-engine-modified");
    Services.obs.addObserver(this, "intl:app-locales-changed");
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified");
      Services.obs.removeObserver(this, "intl:app-locales-changed");
    });

    let suggestsPref = Preferences.get("browser.search.suggest.enabled");
    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    let searchBarPref = Preferences.get("browser.search.widget.inNavBar");
    let privateSuggestsPref = Preferences.get(
      "browser.search.suggest.enabled.private"
    );

    let updateSuggestionCheckboxes =
      this._updateSuggestionCheckboxes.bind(this);
    suggestsPref.on("change", updateSuggestionCheckboxes);
    urlbarSuggestsPref.on("change", updateSuggestionCheckboxes);
    searchBarPref.on("change", updateSuggestionCheckboxes);
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    urlbarSuggests.addEventListener("command", () => {
      urlbarSuggestsPref.value = urlbarSuggests.checked;
    });
    let suggestionsInSearchFieldsCheckbox = document.getElementById(
      "suggestionsInSearchFieldsCheckbox"
    );
    // We only want to call _updateSuggestionCheckboxes once after updating
    // all prefs.
    suggestionsInSearchFieldsCheckbox.addEventListener("command", () => {
      this._skipUpdateSuggestionCheckboxesFromPrefChanges = true;
      if (!searchBarPref.value) {
        urlbarSuggestsPref.value = suggestionsInSearchFieldsCheckbox.checked;
      }
      suggestsPref.value = suggestionsInSearchFieldsCheckbox.checked;
      this._skipUpdateSuggestionCheckboxesFromPrefChanges = false;
      this._updateSuggestionCheckboxes();
    });
    let privateWindowCheckbox = document.getElementById(
      "showSearchSuggestionsPrivateWindows"
    );
    privateWindowCheckbox.addEventListener("command", () => {
      privateSuggestsPref.value = privateWindowCheckbox.checked;
    });

    setEventListener(
      "browserSeparateDefaultEngine",
      "command",
      this._onBrowserSeparateDefaultEngineChange.bind(this)
    );

    this._initDefaultEngines();
    this._initShowSearchTermsCheckbox();
    this._updateSuggestionCheckboxes();
    this._initRecentSeachesCheckbox();
    this._initAddressBar();
  },

  /**
   * Initialize the default engine handling. This will hide the private default
   * options if they are not enabled yet.
   */
  _initDefaultEngines() {
    this._separatePrivateDefaultEnabledPref = Preferences.get(
      "browser.search.separatePrivateDefault.ui.enabled"
    );

    this._separatePrivateDefaultPref = Preferences.get(
      "browser.search.separatePrivateDefault"
    );

    const checkbox = document.getElementById("browserSeparateDefaultEngine");
    checkbox.checked = !this._separatePrivateDefaultPref.value;

    this._updatePrivateEngineDisplayBoxes();

    const listener = () => {
      this._updatePrivateEngineDisplayBoxes();
      this._engineStore.notifyRebuildViews();
    };

    this._separatePrivateDefaultEnabledPref.on("change", listener);
    this._separatePrivateDefaultPref.on("change", listener);
  },

  _initShowSearchTermsCheckbox() {
    let checkbox = document.getElementById("searchShowSearchTermCheckbox");

    // Add Nimbus event to show/hide checkbox.
    let onNimbus = () => {
      checkbox.hidden = !UrlbarPrefs.get("showSearchTermsFeatureGate");
    };
    NimbusFeatures.urlbar.onUpdate(onNimbus);

    // Add observer of Search Bar preference as showSearchTerms
    // can't be shown/hidden while Search Bar is enabled.
    let searchBarPref = Preferences.get("browser.search.widget.inNavBar");
    let updateCheckboxHidden = () => {
      checkbox.hidden =
        !UrlbarPrefs.get("showSearchTermsFeatureGate") || searchBarPref.value;
    };
    searchBarPref.on("change", updateCheckboxHidden);

    // Fire once to initialize.
    onNimbus();
    updateCheckboxHidden();

    window.addEventListener("unload", () => {
      NimbusFeatures.urlbar.offUpdate(onNimbus);
    });
  },

  _updatePrivateEngineDisplayBoxes() {
    const separateEnabled = this._separatePrivateDefaultEnabledPref.value;
    document.getElementById("browserSeparateDefaultEngine").hidden =
      !separateEnabled;

    const separateDefault = this._separatePrivateDefaultPref.value;

    const vbox = document.getElementById("browserPrivateEngineSelection");
    vbox.hidden = !separateEnabled || !separateDefault;
  },

  _onBrowserSeparateDefaultEngineChange(event) {
    this._separatePrivateDefaultPref.value = !event.target.checked;
  },

  _updateSuggestionCheckboxes() {
    if (this._skipUpdateSuggestionCheckboxesFromPrefChanges) {
      return;
    }
    let suggestsPref = Preferences.get("browser.search.suggest.enabled");
    let permanentPB = Services.prefs.getBoolPref(
      "browser.privatebrowsing.autostart"
    );
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    let suggestionsInSearchFieldsCheckbox = document.getElementById(
      "suggestionsInSearchFieldsCheckbox"
    );
    let positionCheckbox = document.getElementById(
      "showSearchSuggestionsFirstCheckbox"
    );
    let privateWindowCheckbox = document.getElementById(
      "showSearchSuggestionsPrivateWindows"
    );
    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    let searchBarPref = Preferences.get("browser.search.widget.inNavBar");

    suggestionsInSearchFieldsCheckbox.checked =
      suggestsPref.value &&
      (searchBarPref.value ? true : urlbarSuggestsPref.value);

    urlbarSuggests.disabled = !suggestsPref.value || permanentPB;
    urlbarSuggests.hidden = !searchBarPref.value;

    privateWindowCheckbox.disabled = !suggestsPref.value;
    privateWindowCheckbox.checked = Preferences.get(
      "browser.search.suggest.enabled.private"
    ).value;
    if (privateWindowCheckbox.disabled) {
      privateWindowCheckbox.checked = false;
    }

    urlbarSuggests.checked = urlbarSuggestsPref.value;
    if (urlbarSuggests.disabled) {
      urlbarSuggests.checked = false;
    }
    if (urlbarSuggests.checked) {
      positionCheckbox.disabled = false;
      // Update the checked state of the show-suggestions-first checkbox.  Note
      // that this does *not* also update its pref, it only checks the box.
      positionCheckbox.checked = Preferences.get(
        positionCheckbox.getAttribute("preference")
      ).value;
    } else {
      positionCheckbox.disabled = true;
      positionCheckbox.checked = false;
    }
    if (
      suggestionsInSearchFieldsCheckbox.checked &&
      !searchBarPref.value &&
      !urlbarSuggests.checked
    ) {
      urlbarSuggestsPref.value = true;
    }

    let permanentPBLabel = document.getElementById(
      "urlBarSuggestionPermanentPBLabel"
    );
    permanentPBLabel.hidden = urlbarSuggests.hidden || !permanentPB;

    this._updateTrendingCheckbox(!suggestsPref.value || permanentPB);
  },

  _initRecentSeachesCheckbox() {
    this._recentSearchesEnabledPref = Preferences.get(
      "browser.urlbar.recentsearches.featureGate"
    );
    let recentSearchesCheckBox = document.getElementById(
      "enableRecentSearches"
    );
    const listener = () => {
      recentSearchesCheckBox.hidden = !this._recentSearchesEnabledPref.value;
    };

    this._recentSearchesEnabledPref.on("change", listener);
    listener();
  },

  async _updateTrendingCheckbox(suggestDisabled) {
    let trendingBox = document.getElementById("showTrendingSuggestionsBox");
    let trendingCheckBox = document.getElementById("showTrendingSuggestions");
    let trendingSupported = (
      await Services.search.getDefault()
    ).supportsResponseType(lazy.SearchUtils.URL_TYPE.TRENDING_JSON);
    let trendingEnabled = Preferences.get(
      "browser.urlbar.trending.featureGate"
    ).value;
    let enabledLocales = Preferences.get(
      "browser.urlbar.trending.enabledLocales"
    ).value;
    if (trendingEnabled && enabledLocales) {
      trendingEnabled = enabledLocales.includes(
        Services.locale.appLocaleAsBCP47
      );
    }
    trendingBox.hidden = !trendingEnabled;
    trendingCheckBox.disabled = suggestDisabled || !trendingSupported;
  },

  // ADDRESS BAR

  /**
   * Initializes the address bar section.
   */
  _initAddressBar() {
    // Update the Firefox Suggest section when its Nimbus config changes.
    let onNimbus = () => this._updateFirefoxSuggestSection();
    NimbusFeatures.urlbar.onUpdate(onNimbus);
    window.addEventListener("unload", () => {
      NimbusFeatures.urlbar.offUpdate(onNimbus);
    });

    // The Firefox Suggest info box potentially needs updating when any of the
    // toggles change.
    let infoBoxPrefs = [
      "browser.urlbar.suggest.quicksuggest.nonsponsored",
      "browser.urlbar.suggest.quicksuggest.sponsored",
      "browser.urlbar.quicksuggest.dataCollection.enabled",
    ];
    for (let pref of infoBoxPrefs) {
      Preferences.get(pref).on("change", () =>
        this._updateFirefoxSuggestInfoBox()
      );
    }

    document.getElementById("clipboardSuggestion").hidden = !UrlbarPrefs.get(
      "clipboard.featureGate"
    );

    this._updateFirefoxSuggestSection(true);
    this._initQuickActionsSection();
  },

  /**
   * Updates the Firefox Suggest section (in the address bar section) depending
   * on whether the user is enrolled in a Firefox Suggest rollout.
   *
   * @param {boolean} [onInit]
   *   Pass true when calling this when initializing the pane.
   */
  _updateFirefoxSuggestSection(onInit = false) {
    let container = document.getElementById("firefoxSuggestContainer");

    if (UrlbarPrefs.get("quickSuggestEnabled")) {
      // Update the l10n IDs of text elements.
      let l10nIdByElementId = {
        locationBarGroupHeader: "addressbar-header-firefox-suggest",
        locationBarSuggestionLabel: "addressbar-suggest-firefox-suggest",
      };
      for (let [elementId, l10nId] of Object.entries(l10nIdByElementId)) {
        let element = document.getElementById(elementId);
        element.dataset.l10nIdOriginal ??= element.dataset.l10nId;
        element.dataset.l10nId = l10nId;
      }

      // Show the container.
      this._updateFirefoxSuggestInfoBox();

      this._updateDismissedSuggestionsStatus();
      Preferences.get(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST).on("change", () =>
        this._updateDismissedSuggestionsStatus()
      );
      Preferences.get(PREF_URLBAR_WEATHER_USER_ENABLED).on("change", () =>
        this._updateDismissedSuggestionsStatus()
      );
      setEventListener("restoreDismissedSuggestions", "command", () =>
        this.restoreDismissedSuggestions()
      );

      container.removeAttribute("hidden");
    } else if (!onInit) {
      // Firefox Suggest is not enabled. This is the default, so to avoid
      // accidentally messing anything up, only modify the doc if we're being
      // called due to a change in the rollout-enabled status (!onInit).
      container.setAttribute("hidden", "true");
      let elementIds = ["locationBarGroupHeader", "locationBarSuggestionLabel"];
      for (let id of elementIds) {
        let element = document.getElementById(id);
        element.dataset.l10nId = element.dataset.l10nIdOriginal;
        delete element.dataset.l10nIdOriginal;
        document.l10n.translateElements([element]);
      }
    }
  },

  /**
   * Updates the Firefox Suggest info box (in the address bar section) depending
   * on the states of the Firefox Suggest toggles.
   */
  _updateFirefoxSuggestInfoBox() {
    let nonsponsored = Preferences.get(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ).value;
    let sponsored = Preferences.get(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ).value;
    let dataCollection = Preferences.get(
      "browser.urlbar.quicksuggest.dataCollection.enabled"
    ).value;

    // Get the l10n ID of the appropriate text based on the values of the three
    // prefs.
    let l10nId;
    if (nonsponsored && sponsored && dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-all";
    } else if (nonsponsored && sponsored && !dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-nonsponsored-sponsored";
    } else if (nonsponsored && !sponsored && dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-nonsponsored-data";
    } else if (nonsponsored && !sponsored && !dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-nonsponsored";
    } else if (!nonsponsored && sponsored && dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-sponsored-data";
    } else if (!nonsponsored && sponsored && !dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-sponsored";
    } else if (!nonsponsored && !sponsored && dataCollection) {
      l10nId = "addressbar-firefox-suggest-info-data";
    }

    let instance = (this._firefoxSuggestInfoBoxInstance = {});
    let infoBox = document.getElementById("firefoxSuggestInfoBox");
    if (!l10nId) {
      infoBox.hidden = true;
    } else {
      let infoText = document.getElementById("firefoxSuggestInfoText");
      infoText.dataset.l10nId = l10nId;

      // If the info box is currently hidden and we unhide it immediately, it
      // will show its old text until the new text is asyncly fetched and shown.
      // That's ugly, so wait for the fetch to finish before unhiding it.
      document.l10n.translateElements([infoText]).then(() => {
        if (instance == this._firefoxSuggestInfoBoxInstance) {
          infoBox.hidden = false;
        }
      });
    }
  },

  _initQuickActionsSection() {
    let showPref = Preferences.get("browser.urlbar.quickactions.showPrefs");
    let showQuickActionsGroup = () => {
      document.getElementById("quickActionsBox").hidden = !showPref.value;
    };
    showPref.on("change", showQuickActionsGroup);
    showQuickActionsGroup();
  },

  /**
   * Enables/disables the "Restore" button for dismissed Firefox Suggest
   * suggestions.
   */
  _updateDismissedSuggestionsStatus() {
    document.getElementById("restoreDismissedSuggestions").disabled =
      !Services.prefs.prefHasUserValue(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST) &&
      !(
        Services.prefs.prefHasUserValue(PREF_URLBAR_WEATHER_USER_ENABLED) &&
        !Services.prefs.getBoolPref(PREF_URLBAR_WEATHER_USER_ENABLED)
      );
  },

  /**
   * Restores Firefox Suggest suggestions dismissed by the user.
   */
  restoreDismissedSuggestions() {
    Services.prefs.clearUserPref(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST);
    Services.prefs.clearUserPref(PREF_URLBAR_WEATHER_USER_ENABLED);
  },

  handleEvent(aEvent) {
    if (aEvent.type != "command") {
      return;
    }
    switch (aEvent.target.id) {
      case "":
        if (aEvent.target.parentNode && aEvent.target.parentNode.parentNode) {
          if (aEvent.target.parentNode.parentNode.id == "defaultEngine") {
            gSearchPane.setDefaultEngine();
          } else if (
            aEvent.target.parentNode.parentNode.id == "defaultPrivateEngine"
          ) {
            gSearchPane.setDefaultPrivateEngine();
          }
        }
        break;
      default:
        gEngineView.handleEvent(aEvent);
    }
  },

  /**
   * Handle when the app locale is changed.
   */
  async appLocalesChanged() {
    await document.l10n.ready;
    await gEngineView.loadL10nNames();
  },

  /**
   * nsIObserver implementation.
   */
  observe(subject, topic, data) {
    switch (topic) {
      case "intl:app-locales-changed": {
        this.appLocalesChanged();
        break;
      }
      case "browser-search-engine-modified": {
        let engine = subject.QueryInterface(Ci.nsISearchEngine);
        switch (data) {
          case "engine-default": {
            // Pass through to the engine store to handle updates.
            this._engineStore.browserSearchEngineModified(engine, data);
            gSearchPane._updateSuggestionCheckboxes();
            break;
          }
          default:
            this._engineStore.browserSearchEngineModified(engine, data);
        }
      }
    }
  },

  showRestoreDefaults(aEnable) {
    document.getElementById("restoreDefaultSearchEngines").disabled = !aEnable;
  },

  async setDefaultEngine() {
    await Services.search.setDefault(
      document.getElementById("defaultEngine").selectedItem.engine,
      Ci.nsISearchService.CHANGE_REASON_USER
    );
    if (ExtensionSettingsStore.getSetting(SEARCH_TYPE, SEARCH_KEY) !== null) {
      ExtensionSettingsStore.select(
        ExtensionSettingsStore.SETTING_USER_SET,
        SEARCH_TYPE,
        SEARCH_KEY
      );
    }
  },

  async setDefaultPrivateEngine() {
    await Services.search.setDefaultPrivate(
      document.getElementById("defaultPrivateEngine").selectedItem.engine,
      Ci.nsISearchService.CHANGE_REASON_USER
    );
  },
};

/**
 * Keeps track of the search engine objects and notifies the views for updates.
 */
class EngineStore {
  /**
   * A list of engines that are currently visible in the UI.
   *
   * @type {Object[]}
   */
  engines = [];

  /**
   * A list of application provided engines used when restoring the list of
   * engines to the default set and order.
   *
   * @type {nsISearchEngine[]}
   */
  #appProvidedEngines = [];

  /**
   * A list of listeners to be notified when the engine list changes.
   *
   * @type {Object[]}
   */
  #listeners = [];

  async init() {
    let visibleEngines = await Services.search.getVisibleEngines();
    for (let engine of visibleEngines) {
      this.addEngine(engine);
    }

    let appProvidedEngines = await Services.search.getAppProvidedEngines();
    this.#appProvidedEngines = appProvidedEngines.map(this._cloneEngine, this);

    this.notifyRowCountChanged(0, visibleEngines.length);

    // check if we need to disable the restore defaults button
    var someHidden = this.#appProvidedEngines.some(e => e.hidden);
    gSearchPane.showRestoreDefaults(someHidden);
  }

  /**
   * Adds a listener to be notified when the engine list changes.
   *
   * @param {object} aListener
   */
  addListener(aListener) {
    this.#listeners.push(aListener);
  }

  /**
   * Notifies all listeners that the engine list has changed and they should
   * rebuild.
   */
  notifyRebuildViews() {
    for (let listener of this.#listeners) {
      try {
        listener.rebuild(this.engines);
      } catch (ex) {
        console.error("Error notifying EngineStore listener", ex);
      }
    }
  }

  /**
   * Notifies all listeners that the number of engines in the list has changed.
   *
   * @param {number} index
   * @param {number} count
   */
  notifyRowCountChanged(index, count) {
    for (let listener of this.#listeners) {
      listener.rowCountChanged(index, count, this.engines);
    }
  }

  /**
   * Notifies all listeners that the default engine has changed.
   *
   * @param {string} type
   * @param {object} engine
   */
  notifyDefaultEngineChanged(type, engine) {
    for (let listener of this.#listeners) {
      if ("defaultEngineChanged" in listener) {
        listener.defaultEngineChanged(type, engine, this.engines);
      }
    }
  }

  notifyEngineIconUpdated(engine) {
    // Check the engine is still in the list.
    let index = this._getIndexForEngine(engine);
    if (index != -1) {
      for (let listener of this.#listeners) {
        listener.engineIconUpdated(index, this.engines);
      }
    }
  }

  _getIndexForEngine(aEngine) {
    return this.engines.indexOf(aEngine);
  }

  _getEngineByName(aName) {
    return this.engines.find(engine => engine.name == aName);
  }

  _cloneEngine(aEngine) {
    var clonedObj = {
      iconURL: null,
    };
    for (let i of ["id", "name", "alias", "hidden"]) {
      clonedObj[i] = aEngine[i];
    }
    clonedObj.originalEngine = aEngine;

    // Trigger getting the iconURL for this engine.
    aEngine.getIconURL().then(iconURL => {
      if (iconURL) {
        clonedObj.iconURL = iconURL;
      } else if (window.devicePixelRatio > 1) {
        clonedObj.iconURL =
          "chrome://browser/skin/search-engine-placeholder@2x.png";
      } else {
        clonedObj.iconURL =
          "chrome://browser/skin/search-engine-placeholder.png";
      }

      this.notifyEngineIconUpdated(clonedObj);
    });

    return clonedObj;
  }

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine.id == this.originalEngine.id;
  }

  addEngine(aEngine) {
    this.engines.push(this._cloneEngine(aEngine));
  }

  updateEngine(newEngine) {
    let engineToUpdate = this.engines.findIndex(
      e => e.originalEngine.id == newEngine.id
    );
    if (engineToUpdate != -1) {
      this.engines[engineToUpdate] = this._cloneEngine(newEngine);
    }
  }

  moveEngine(aEngine, aNewIndex) {
    if (aNewIndex < 0 || aNewIndex > this.engines.length - 1) {
      throw new Error("ES_moveEngine: invalid aNewIndex!");
    }
    var index = this._getIndexForEngine(aEngine);
    if (index == -1) {
      throw new Error("ES_moveEngine: invalid engine?");
    }

    if (index == aNewIndex) {
      return Promise.resolve();
    } // nothing to do

    // Move the engine in our internal store
    var removedEngine = this.engines.splice(index, 1)[0];
    this.engines.splice(aNewIndex, 0, removedEngine);

    return Services.search.moveEngine(aEngine.originalEngine, aNewIndex);
  }

  removeEngine(aEngine) {
    if (this.engines.length == 1) {
      throw new Error("Cannot remove last engine!");
    }

    let engineName = aEngine.name;
    let index = this.engines.findIndex(element => element.name == engineName);

    if (index == -1) {
      throw new Error("invalid engine?");
    }

    this.engines.splice(index, 1)[0];

    if (aEngine.isAppProvided) {
      gSearchPane.showRestoreDefaults(true);
    }

    this.notifyRowCountChanged(index, -1);

    document.getElementById("engineList").focus();
  }

  /**
   * Update the default engine UI and engine tree view as appropriate when engine changes
   * or locale changes occur.
   *
   * @param {nsISearchEngine} engine
   * @param {string} data
   */
  browserSearchEngineModified(engine, data) {
    engine.QueryInterface(Ci.nsISearchEngine);
    switch (data) {
      case "engine-added":
        this.addEngine(engine);
        this.notifyRowCountChanged(gEngineView.lastEngineIndex, 1);
        break;
      case "engine-changed":
      case "engine-icon-changed":
        this.updateEngine(engine);
        this.notifyRebuildViews();
        break;
      case "engine-removed":
        this.removeEngine(engine);
        break;
      case "engine-default":
        this.notifyDefaultEngineChanged("normal", engine);
        break;
      case "engine-default-private":
        this.notifyDefaultEngineChanged("private", engine);
        break;
    }
  }

  async restoreDefaultEngines() {
    var added = 0;

    for (var i = 0; i < this.#appProvidedEngines.length; ++i) {
      var e = this.#appProvidedEngines[i];

      // If the engine is already in the list, just move it.
      if (this.engines.some(this._isSameEngine, e)) {
        await this.moveEngine(this._getEngineByName(e.name), i);
      } else {
        // Otherwise, add it back to our internal store

        // The search service removes the alias when an engine is hidden,
        // so clear any alias we may have cached before unhiding the engine.
        e.alias = "";

        this.engines.splice(i, 0, e);
        let engine = e.originalEngine;
        engine.hidden = false;
        await Services.search.moveEngine(engine, i);
        added++;
      }
    }

    // We can't do this as part of the loop above because the indices are
    // used for moving engines.
    let policyRemovedEngineNames =
      Services.policies.getActivePolicies()?.SearchEngines?.Remove || [];
    for (let engineName of policyRemovedEngineNames) {
      let engine = Services.search.getEngineByName(engineName);
      if (engine) {
        try {
          await Services.search.removeEngine(engine);
        } catch (ex) {
          // Engine might not exist
        }
      }
    }

    Services.search.resetToAppDefaultEngine();
    gSearchPane.showRestoreDefaults(false);
    this.notifyRebuildViews();
    return added;
  }

  changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1) {
      throw new Error("invalid engine?");
    }

    this.engines[index][aProp] = aNewValue;
    aEngine.originalEngine[aProp] = aNewValue;
  }
}

/**
 * Manages the view of the Search Shortcuts tree on the search pane of preferences.
 */
class EngineView {
  _engineStore = null;
  _engineList = null;
  tree = null;

  constructor(aEngineStore) {
    this._engineStore = aEngineStore;
    this._engineList = document.getElementById("engineList");
    this._engineList.view = this;

    UrlbarPrefs.addObserver(this);
    aEngineStore.addListener(this);

    this.loadL10nNames();
    this.#addListeners();
    this.#showAddEngineButton();
  }

  loadL10nNames() {
    // This maps local shortcut sources to their l10n names.  The names are needed
    // by getCellText.  Getting the names is async but getCellText is not, so we
    // cache them here to retrieve them syncronously in getCellText.
    this._localShortcutL10nNames = new Map();
    return document.l10n
      .formatValues(
        UrlbarUtils.LOCAL_SEARCH_MODES.map(mode => {
          let name = UrlbarUtils.getResultSourceName(mode.source);
          return { id: `urlbar-search-mode-${name}` };
        })
      )
      .then(names => {
        for (let { source } of UrlbarUtils.LOCAL_SEARCH_MODES) {
          this._localShortcutL10nNames.set(source, names.shift());
        }
        // Invalidate the tree now that we have the names in case getCellText was
        // called before name retrieval finished.
        this.invalidate();
      });
  }

  #addListeners() {
    this._engineList.addEventListener("click", this);
    this._engineList.addEventListener("dragstart", this);
    this._engineList.addEventListener("keypress", this);
    this._engineList.addEventListener("select", this);
    this._engineList.addEventListener("dblclick", this);
  }

  /**
   * Shows the "Add Search Engine" button if the pref is enabled.
   */
  #showAddEngineButton() {
    let aliasRefresh = Services.prefs.getBoolPref(
      "browser.urlbar.update2.engineAliasRefresh",
      false
    );
    if (aliasRefresh) {
      let addButton = document.getElementById("addEngineButton");
      addButton.hidden = false;
    }
  }

  get lastEngineIndex() {
    return this._engineStore.engines.length - 1;
  }

  get selectedIndex() {
    var seln = this.selection;
    if (seln.getRangeCount() > 0) {
      var min = {};
      seln.getRangeAt(0, min, {});
      return min.value;
    }
    return -1;
  }

  get selectedEngine() {
    return this._engineStore.engines[this.selectedIndex];
  }

  // Helpers
  rebuild() {
    this.invalidate();
  }

  rowCountChanged(index, count) {
    if (!this.tree) {
      return;
    }
    this.tree.rowCountChanged(index, count);

    // If we're removing elements, ensure that we still have a selection.
    if (count < 0) {
      this.selection.select(Math.min(index, this.rowCount - 1));
      this.ensureRowIsVisible(this.currentIndex);
    }
  }

  engineIconUpdated(index) {
    this.tree?.invalidateCell(
      index,
      this.tree.columns.getNamedColumn("engineName")
    );
  }

  invalidate() {
    this.tree?.invalidate();
  }

  ensureRowIsVisible(index) {
    this.tree.ensureRowIsVisible(index);
  }

  getSourceIndexFromDrag(dataTransfer) {
    return parseInt(dataTransfer.getData(ENGINE_FLAVOR));
  }

  isCheckBox(index, column) {
    return column.id == "engineShown";
  }

  isEngineSelectedAndRemovable() {
    let defaultEngine = Services.search.defaultEngine;
    let defaultPrivateEngine = Services.search.defaultPrivateEngine;
    // We don't allow the last remaining engine to be removed, thus the
    // `this.lastEngineIndex != 0` check.
    // We don't allow the default engine to be removed.
    return (
      this.selectedIndex != -1 &&
      this.lastEngineIndex != 0 &&
      !this._getLocalShortcut(this.selectedIndex) &&
      this.selectedEngine.name != defaultEngine.name &&
      this.selectedEngine.name != defaultPrivateEngine.name
    );
  }

  /**
   * Returns the local shortcut corresponding to a tree row, or null if the row
   * is not a local shortcut.
   *
   * @param {number} index
   *   The tree row index.
   * @returns {object}
   *   The local shortcut object or null if the row is not a local shortcut.
   */
  _getLocalShortcut(index) {
    let engineCount = this._engineStore.engines.length;
    if (index < engineCount) {
      return null;
    }
    return UrlbarUtils.LOCAL_SEARCH_MODES[index - engineCount];
  }

  /**
   * Called by UrlbarPrefs when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to the browser.urlbar branch.
   */
  onPrefChanged(pref) {
    // If one of the local shortcut prefs was toggled, toggle its row's
    // checkbox.
    let parts = pref.split(".");
    if (parts[0] == "shortcuts" && parts[1] && parts.length == 2) {
      this.invalidate();
    }
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dblclick":
        if (aEvent.target.id == "engineChildren") {
          let cell = aEvent.target.parentNode.getCellAt(
            aEvent.clientX,
            aEvent.clientY
          );
          if (cell.col?.id == "engineKeyword") {
            this.#startEditingAlias(this.selectedIndex);
          }
        }
        break;
      case "click":
        if (
          aEvent.target.id != "engineChildren" &&
          !aEvent.target.classList.contains("searchEngineAction")
        ) {
          // We don't want to toggle off selection while editing keyword
          // so proceed only when the input field is hidden.
          // We need to check that engineList.view is defined here
          // because the "click" event listener is on <window> and the
          // view might have been destroyed if the pane has been navigated
          // away from.
          if (this._engineList.inputField.hidden && this._engineList.view) {
            let selection = this._engineList.view.selection;
            if (selection?.count > 0) {
              selection.toggleSelect(selection.currentIndex);
            }
            this._engineList.blur();
          }
        }
        break;
      case "command":
        switch (aEvent.target.id) {
          case "restoreDefaultSearchEngines":
            this.#onRestoreDefaults();
            break;
          case "removeEngineButton":
            Services.search.removeEngine(this.selectedEngine.originalEngine);
            break;
          case "addEngineButton":
            gSubDialog.open(
              "chrome://browser/content/preferences/dialogs/addEngine.xhtml",
              { features: "resizable=no, modal=yes" }
            );
            break;
        }
        break;
      case "dragstart":
        if (aEvent.target.id == "engineChildren") {
          this.#onDragEngineStart(aEvent);
        }
        break;
      case "keypress":
        if (aEvent.target.id == "engineList") {
          this.#onTreeKeyPress(aEvent);
        }
        break;
      case "select":
        if (aEvent.target.id == "engineList") {
          this.#onTreeSelect();
        }
        break;
    }
  }

  /**
   * Called when the restore default engines button is clicked to reset the
   * list of engines to their defaults.
   */
  async #onRestoreDefaults() {
    let num = await this._engineStore.restoreDefaultEngines();
    this.rowCountChanged(0, num);
  }

  #onDragEngineStart(event) {
    let selectedIndex = this.selectedIndex;

    // Local shortcut rows can't be dragged or re-ordered.
    if (this._getLocalShortcut(selectedIndex)) {
      event.preventDefault();
      return;
    }

    let tree = document.getElementById("engineList");
    let cell = tree.getCellAt(event.clientX, event.clientY);
    if (selectedIndex >= 0 && !this.isCheckBox(cell.row, cell.col)) {
      event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
      event.dataTransfer.effectAllowed = "move";
    }
  }

  #onTreeSelect() {
    document.getElementById("removeEngineButton").disabled =
      !this.isEngineSelectedAndRemovable();
  }

  #onTreeKeyPress(aEvent) {
    let index = this.selectedIndex;
    let tree = document.getElementById("engineList");
    if (tree.hasAttribute("editing")) {
      return;
    }

    if (aEvent.charCode == KeyEvent.DOM_VK_SPACE) {
      // Space toggles the checkbox.
      let newValue = !this.getCellValue(
        index,
        tree.columns.getNamedColumn("engineShown")
      );
      this.setCellValue(
        index,
        tree.columns.getFirstColumn(),
        newValue.toString()
      );
      // Prevent page from scrolling on the space key.
      aEvent.preventDefault();
    } else {
      let isMac = Services.appinfo.OS == "Darwin";
      if (
        (isMac && aEvent.keyCode == KeyEvent.DOM_VK_RETURN) ||
        (!isMac && aEvent.keyCode == KeyEvent.DOM_VK_F2)
      ) {
        this.#startEditingAlias(index);
      } else if (
        aEvent.keyCode == KeyEvent.DOM_VK_DELETE ||
        (isMac &&
          aEvent.shiftKey &&
          aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE &&
          this.isEngineSelectedAndRemovable())
      ) {
        // Delete and Shift+Backspace (Mac) removes selected engine.
        Services.search.removeEngine(this.selectedEngine.originalEngine);
      }
    }
  }

  /**
   * Triggers editing of an alias in the tree.
   *
   * @param {number} index
   */
  #startEditingAlias(index) {
    // Local shortcut aliases can't be edited.
    if (this._getLocalShortcut(index)) {
      return;
    }

    let tree = document.getElementById("engineList");
    let engine = this._engineStore.engines[index];
    tree.startEditing(index, tree.columns.getLastColumn());
    tree.inputField.value = engine.alias || "";
    tree.inputField.select();
  }

  // nsITreeView
  get rowCount() {
    return (
      this._engineStore.engines.length + UrlbarUtils.LOCAL_SEARCH_MODES.length
    );
  }

  getImageSrc(index, column) {
    if (column.id == "engineName") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return shortcut.icon;
      }

      return this._engineStore.engines[index].iconURL;
    }

    return "";
  }

  getCellText(index, column) {
    if (column.id == "engineName") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return this._localShortcutL10nNames.get(shortcut.source) || "";
      }
      return this._engineStore.engines[index].name;
    } else if (column.id == "engineKeyword") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return shortcut.restrict;
      }
      return this._engineStore.engines[index].originalEngine.aliases.join(", ");
    }
    return "";
  }

  setTree(tree) {
    this.tree = tree;
  }

  canDrop(targetIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    return (
      sourceIndex != -1 &&
      sourceIndex != targetIndex &&
      sourceIndex != targetIndex + orientation &&
      // Local shortcut rows can't be dragged or dropped on.
      targetIndex < this._engineStore.engines.length
    );
  }

  async drop(dropIndex, orientation, dataTransfer) {
    // Local shortcut rows can't be dragged or dropped on.  This can sometimes
    // be reached even though canDrop returns false for these rows.
    if (this._engineStore.engines.length <= dropIndex) {
      return;
    }

    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    var sourceEngine = this._engineStore.engines[sourceIndex];

    const nsITreeView = Ci.nsITreeView;
    if (dropIndex > sourceIndex) {
      if (orientation == nsITreeView.DROP_BEFORE) {
        dropIndex--;
      }
    } else if (orientation == nsITreeView.DROP_AFTER) {
      dropIndex++;
    }

    await this._engineStore.moveEngine(sourceEngine, dropIndex);
    gSearchPane.showRestoreDefaults(true);

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  }

  selection = null;
  getRowProperties() {
    return "";
  }
  getCellProperties(index, column) {
    if (column.id == "engineName") {
      // For local shortcut rows, return the result source name so we can style
      // the icons in CSS.
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return UrlbarUtils.getResultSourceName(shortcut.source);
      }
    }
    return "";
  }
  getColumnProperties() {
    return "";
  }
  isContainer() {
    return false;
  }
  isContainerOpen() {
    return false;
  }
  isContainerEmpty() {
    return false;
  }
  isSeparator() {
    return false;
  }
  isSorted() {
    return false;
  }
  getParentIndex() {
    return -1;
  }
  hasNextSibling() {
    return false;
  }
  getLevel() {
    return 0;
  }
  getCellValue(index, column) {
    if (column.id == "engineShown") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return UrlbarPrefs.get(shortcut.pref);
      }
      return !this._engineStore.engines[index].originalEngine.hideOneOffButton;
    }
    return undefined;
  }
  toggleOpenState() {}
  cycleHeader() {}
  selectionChanged() {}
  cycleCell() {}
  isEditable(index, column) {
    return (
      column.id != "engineName" &&
      (column.id == "engineShown" || !this._getLocalShortcut(index))
    );
  }
  setCellValue(index, column, value) {
    if (column.id == "engineShown") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        UrlbarPrefs.set(shortcut.pref, value == "true");
        this.invalidate();
        return;
      }
      this._engineStore.engines[index].originalEngine.hideOneOffButton =
        value != "true";
      this.invalidate();
    }
  }
  setCellText(index, column, value) {
    if (column.id == "engineKeyword") {
      this.#changeKeyword(this._engineStore.engines[index], value).then(
        valid => {
          if (!valid) {
            this.#startEditingAlias(index);
          }
        }
      );
    }
  }

  /**
   * Handles changing the keyword for an engine. This will check for potentially
   * duplicate keywords and prompt the user if necessary.
   *
   * @param {object} aEngine
   *   The engine to change.
   * @param {string} aNewKeyword
   *   The new keyword.
   * @returns {Promise<boolean>}
   *   Resolves to true if the keyword was changed.
   */
  async #changeKeyword(aEngine, aNewKeyword) {
    let keyword = aNewKeyword.trim();
    if (keyword) {
      let eduplicate = false;
      let dupName = "";

      // Check for duplicates in Places keywords.
      let bduplicate = !!(await PlacesUtils.keywords.fetch(keyword));

      // Check for duplicates in changes we haven't committed yet
      let engines = this._engineStore.engines;
      let lc_keyword = keyword.toLocaleLowerCase();
      for (let engine of engines) {
        if (
          engine.alias &&
          engine.alias.toLocaleLowerCase() == lc_keyword &&
          engine.name != aEngine.name
        ) {
          eduplicate = true;
          dupName = engine.name;
          break;
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        let msgids = [{ id: "search-keyword-warning-title" }];
        if (eduplicate) {
          msgids.push({
            id: "search-keyword-warning-engine",
            args: { name: dupName },
          });
        } else {
          msgids.push({ id: "search-keyword-warning-bookmark" });
        }

        let [dtitle, msg] = await document.l10n.formatValues(msgids);

        Services.prompt.alert(window, dtitle, msg);
        return false;
      }
    }

    this._engineStore.changeEngine(aEngine, "alias", keyword);
    this.invalidate();
    return true;
  }
}

/**
 * Manages the default engine dropdown buttons in the search pane of preferences.
 */
class DefaultEngineDropDown {
  #element = null;
  #type = null;

  constructor(type, engineStore) {
    this.#type = type;
    this.#element = document.getElementById(
      type == "private" ? "defaultPrivateEngine" : "defaultEngine"
    );

    engineStore.addListener(this);
  }

  rowCountChanged(index, count, enginesList) {
    // Simply rebuild the menulist, rather than trying to update the changed row.
    this.rebuild(enginesList);
  }

  defaultEngineChanged(type, engine, enginesList) {
    if (type != this.#type) {
      return;
    }
    // If the user is going through the drop down using up/down keys, the
    // dropdown may still be open (eg. on Windows) when engine-default is
    // fired, so rebuilding the list unconditionally would get in the way.
    let selectedEngineName = this.#element.selectedItem?.engine?.name;
    if (selectedEngineName != engine.name) {
      this.rebuild(enginesList);
    }
  }

  engineIconUpdated(index, enginesList) {
    let item = this.#element.getItemAtIndex(index);
    // Check this is the right item.
    if (item?.label == enginesList[index].name) {
      item.setAttribute("image", enginesList[index].iconURL);
    }
  }

  async rebuild(enginesList) {
    if (
      this.#type == "private" &&
      !gSearchPane._separatePrivateDefaultPref.value
    ) {
      return;
    }
    let defaultEngine = await Services.search[
      this.#type == "normal" ? "getDefault" : "getDefaultPrivate"
    ]();

    this.#element.removeAllItems();
    for (let engine of enginesList) {
      let item = this.#element.appendItem(engine.name);
      item.setAttribute(
        "class",
        "menuitem-iconic searchengine-menuitem menuitem-with-favicon"
      );
      if (engine.iconURL) {
        item.setAttribute("image", engine.iconURL);
      }
      item.engine = engine;
      if (engine.name == defaultEngine.name) {
        this.#element.selectedItem = item;
      }
    }
    // This should never happen, but try and make sure we have at least one
    // selected item.
    if (!this.#element.selectedItem) {
      this.#element.selectedIndex = 0;
    }
  }
}
