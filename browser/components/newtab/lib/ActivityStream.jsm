/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

// NB: Eagerly load modules that will be loaded/constructed/initialized in the
// common case to avoid the overhead of wrapping and detecting lazy loading.
const { actionCreators: ac, actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AboutPreferences",
  "resource://activity-stream/lib/AboutPreferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DefaultPrefs",
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NewTabInit",
  "resource://activity-stream/lib/NewTabInit.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SectionsFeed",
  "resource://activity-stream/lib/SectionsManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "RecommendationProviderSwitcher",
  "resource://activity-stream/lib/RecommendationProviderSwitcher.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesFeed",
  "resource://activity-stream/lib/PlacesFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrefsFeed",
  "resource://activity-stream/lib/PrefsFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Store",
  "resource://activity-stream/lib/Store.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SystemTickFeed",
  "resource://activity-stream/lib/SystemTickFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryFeed",
  "resource://activity-stream/lib/TelemetryFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FaviconFeed",
  "resource://activity-stream/lib/FaviconFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TopSitesFeed",
  "resource://activity-stream/lib/TopSitesFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TopStoriesFeed",
  "resource://activity-stream/lib/TopStoriesFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HighlightsFeed",
  "resource://activity-stream/lib/HighlightsFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ASRouterFeed",
  "resource://activity-stream/lib/ASRouterFeed.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DiscoveryStreamFeed",
  "resource://activity-stream/lib/DiscoveryStreamFeed.jsm"
);

const DEFAULT_SITES = new Map([
  // This first item is the global list fallback for any unexpected geos
  [
    "",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.reddit.com/,https://www.amazon.com/,https://twitter.com/",
  ],
  [
    "US",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/",
  ],
  [
    "CA",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://www.amazon.ca/,https://twitter.com/",
  ],
  [
    "DE",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.de/,https://www.ebay.de/,https://www.wikipedia.org/,https://www.reddit.com/",
  ],
  [
    "PL",
    "https://www.youtube.com/,https://www.facebook.com/,https://allegro.pl/,https://www.wikipedia.org/,https://www.olx.pl/,https://www.wykop.pl/",
  ],
  [
    "RU",
    "https://vk.com/,https://www.youtube.com/,https://ok.ru/,https://www.avito.ru/,https://www.aliexpress.com/,https://www.wikipedia.org/",
  ],
  [
    "GB",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.amazon.co.uk/,https://www.bbc.co.uk/,https://www.ebay.co.uk/",
  ],
  [
    "FR",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.amazon.fr/,https://www.leboncoin.fr/,https://twitter.com/",
  ],
  [
    "CN",
    "https://www.baidu.com/,https://www.zhihu.com/,https://www.ifeng.com/,https://weibo.com/,https://www.ctrip.com/,https://www.iqiyi.com/",
  ],
]);
const GEO_PREF = "browser.search.region";
const REGION_STORIES_CONFIG =
  "browser.newtabpage.activity-stream.discoverystream.region-stories-config";
const REGION_SPOCS_CONFIG =
  "browser.newtabpage.activity-stream.discoverystream.region-spocs-config";
const REGION_LAYOUT_CONFIG =
  "browser.newtabpage.activity-stream.discoverystream.region-layout-config";

// Determine if spocs should be shown for a geo/locale
function showSpocs({ geo }) {
  const spocsGeoString =
    Services.prefs.getStringPref(REGION_SPOCS_CONFIG) || "";
  const spocsGeo = spocsGeoString.split(",").map(s => s.trim());
  return spocsGeo.includes(geo);
}

// Configure default Activity Stream prefs with a plain `value` or a `getValue`
// that computes a value. A `value_local_dev` is used for development defaults.
const PREFS_CONFIG = new Map([
  [
    "default.sites",
    {
      title:
        "Comma-separated list of default top sites to fill in behind visited sites",
      getValue: ({ geo }) =>
        DEFAULT_SITES.get(DEFAULT_SITES.has(geo) ? geo : ""),
    },
  ],
  [
    "feeds.section.topstories.options",
    {
      title: "Configuration options for top stories feed",
      // This is a dynamic pref as it depends on the feed being shown or not
      getValue: args =>
        JSON.stringify({
          api_key_pref: "extensions.pocket.oAuthConsumerKey",
          // Use the opposite value as what default value the feed would have used
          hidden: !PREFS_CONFIG.get("feeds.section.topstories").getValue(args),
          provider_icon: "pocket",
          provider_name: "Pocket",
          read_more_endpoint:
            "https://getpocket.com/explore/trending?src=fx_new_tab",
          stories_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=3&consumer_key=$apiKey&locale_lang=${
            args.locale
          }&feed_variant=${
            showSpocs(args) ? "default_spocs_on" : "default_spocs_off"
          }`,
          stories_referrer: "https://getpocket.com/recommendations",
          topics_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/trending-topics?version=2&consumer_key=$apiKey&locale_lang=${args.locale}`,
          model_keys: [
            "nmf_model_animals",
            "nmf_model_business",
            "nmf_model_career",
            "nmf_model_datascience",
            "nmf_model_design",
            "nmf_model_education",
            "nmf_model_entertainment",
            "nmf_model_environment",
            "nmf_model_fashion",
            "nmf_model_finance",
            "nmf_model_food",
            "nmf_model_health",
            "nmf_model_home",
            "nmf_model_life",
            "nmf_model_marketing",
            "nmf_model_politics",
            "nmf_model_programming",
            "nmf_model_science",
            "nmf_model_shopping",
            "nmf_model_sports",
            "nmf_model_tech",
            "nmf_model_travel",
            "nb_model_animals",
            "nb_model_books",
            "nb_model_business",
            "nb_model_career",
            "nb_model_datascience",
            "nb_model_design",
            "nb_model_economics",
            "nb_model_education",
            "nb_model_entertainment",
            "nb_model_environment",
            "nb_model_fashion",
            "nb_model_finance",
            "nb_model_food",
            "nb_model_game",
            "nb_model_health",
            "nb_model_history",
            "nb_model_home",
            "nb_model_life",
            "nb_model_marketing",
            "nb_model_military",
            "nb_model_philosophy",
            "nb_model_photography",
            "nb_model_politics",
            "nb_model_productivity",
            "nb_model_programming",
            "nb_model_psychology",
            "nb_model_science",
            "nb_model_shopping",
            "nb_model_society",
            "nb_model_space",
            "nb_model_sports",
            "nb_model_tech",
            "nb_model_travel",
            "nb_model_writing",
          ],
          show_spocs: showSpocs(args),
          personalized: true,
          version: 1,
        }),
    },
  ],
  [
    "showSponsored",
    {
      title:
        "Show sponsored cards in spoc experiment (show_spocs in topstories.options has to be set to true as well)",
      value: true,
    },
  ],
  [
    "pocketCta",
    {
      title: "Pocket cta and button for logged out users.",
      value: JSON.stringify({
        cta_button: "",
        cta_text: "",
        cta_url: "",
        use_cta: false,
      }),
    },
  ],
  [
    "filterAdult",
    {
      title: "Remove adult pages from sites, highlights, etc.",
      value: true,
    },
  ],
  [
    "showSearch",
    {
      title: "Show the Search bar",
      value: true,
    },
  ],
  [
    "feeds.snippets",
    {
      title: "Show snippets on activity stream",
      value: true,
    },
  ],
  [
    "topSitesRows",
    {
      title: "Number of rows of Top Sites to display",
      value: 1,
    },
  ],
  [
    "telemetry",
    {
      title: "Enable system error and usage data collection",
      value: true,
      value_local_dev: false,
    },
  ],
  [
    "telemetry.ut.events",
    {
      title: "Enable Unified Telemetry event data collection",
      value: AppConstants.EARLY_BETA_OR_EARLIER,
      value_local_dev: false,
    },
  ],
  [
    "telemetry.structuredIngestion",
    {
      title: "Enable Structured Ingestion Telemetry data collection",
      value: true,
      value_local_dev: false,
    },
  ],
  [
    "telemetry.structuredIngestion.endpoint",
    {
      title: "Structured Ingestion telemetry server endpoint",
      value: "https://incoming.telemetry.mozilla.org/submit",
    },
  ],
  [
    "section.highlights.includeVisited",
    {
      title:
        "Boolean flag that decides whether or not to show visited pages in highlights.",
      value: true,
    },
  ],
  [
    "section.highlights.includeBookmarks",
    {
      title:
        "Boolean flag that decides whether or not to show bookmarks in highlights.",
      value: true,
    },
  ],
  [
    "section.highlights.includePocket",
    {
      title:
        "Boolean flag that decides whether or not to show saved Pocket stories in highlights.",
      value: true,
    },
  ],
  [
    "section.highlights.includeDownloads",
    {
      title:
        "Boolean flag that decides whether or not to show saved recent Downloads in highlights.",
      value: true,
    },
  ],
  [
    "section.highlights.rows",
    {
      title: "Number of rows of Highlights to display",
      value: 1,
    },
  ],
  [
    "section.topstories.rows",
    {
      title: "Number of rows of Top Stories to display",
      value: 1,
    },
  ],
  [
    "sectionOrder",
    {
      title: "The rendering order for the sections",
      value: "topsites,topstories,highlights",
    },
  ],
  [
    "improvesearch.noDefaultSearchTile",
    {
      title: "Remove tiles that are the same as the default search",
      value: true,
    },
  ],
  [
    "improvesearch.topSiteSearchShortcuts.searchEngines",
    {
      title:
        "An ordered, comma-delimited list of search shortcuts that we should try and pin",
      // This pref is dynamic as the shortcuts vary depending on the region
      getValue: ({ geo }) => {
        if (!geo) {
          return "";
        }
        const searchShortcuts = [];
        if (geo === "CN") {
          searchShortcuts.push("baidu");
        } else if (["BY", "KZ", "RU", "TR"].includes(geo)) {
          searchShortcuts.push("yandex");
        } else {
          searchShortcuts.push("google");
        }
        if (["DE", "FR", "GB", "IT", "JP", "US"].includes(geo)) {
          searchShortcuts.push("amazon");
        }
        return searchShortcuts.join(",");
      },
    },
  ],
  [
    "improvesearch.topSiteSearchShortcuts.havePinned",
    {
      title:
        "A comma-delimited list of search shortcuts that have previously been pinned",
      value: "",
    },
  ],
  [
    "asrouter.devtoolsEnabled",
    {
      title: "Are the asrouter devtools enabled?",
      value: false,
    },
  ],
  [
    "asrouter.userprefs.cfr.addons",
    {
      title: "Does the user allow CFR addon recommendations?",
      value: true,
    },
  ],
  [
    "asrouter.userprefs.cfr.features",
    {
      title: "Does the user allow CFR feature recommendations?",
      value: true,
    },
  ],
  [
    "asrouter.providers.onboarding",
    {
      title: "Configuration for onboarding provider",
      value: JSON.stringify({
        id: "onboarding",
        type: "local",
        localProvider: "OnboardingMessageProvider",
        enabled: true,
        // Block specific messages from this local provider
        exclude: [],
      }),
    },
  ],
  [
    "asrouter.providers.cfr-fxa",
    {
      title: "Configuration for CFR FxA Messages provider",
      value: JSON.stringify({
        id: "cfr-fxa",
        enabled: true,
        type: "remote-settings",
        bucket: "cfr-fxa",
        frequency: { custom: [{ period: "daily", cap: 1 }] },
        updateCycleInMs: 3600000,
      }),
    },
  ],
  // See browser/app/profile/firefox.js for other ASR preferences. They must be defined there to enable roll-outs.
  [
    "discoverystream.flight.blocks",
    {
      title: "Track flight blocks",
      skipBroadcast: true,
      value: "{}",
    },
  ],
  [
    "discoverystream.config",
    {
      title: "Configuration for the new pocket new tab",
      getValue: ({ geo, locale }) => {
        return JSON.stringify({
          api_key_pref: "extensions.pocket.oAuthConsumerKey",
          collapsible: true,
          enabled: true,
          show_spocs: showSpocs({ geo }),
          hardcoded_layout: true,
          personalized: true,
          // This is currently an exmple layout used for dev purposes.
          layout_endpoint:
            "https://getpocket.cdn.mozilla.net/v3/newtab/layout?version=1&consumer_key=$apiKey&layout_variant=basic",
        });
      },
    },
  ],
  [
    "discoverystream.endpoints",
    {
      title:
        "Endpoint prefixes (comma-separated) that are allowed to be requested",
      value: "https://getpocket.cdn.mozilla.net/,https://spocs.getpocket.com/",
    },
  ],
  [
    "discoverystream.engagementLabelEnabled",
    {
      title:
        "Allow the display of engagement labels for discovery stream components (eg: Trending, Popular, etc)",
      value: false,
    },
  ],
  [
    "discoverystream.isCollectionDismissible",
    {
      title: "Allows Pocket story collections to be dismissed",
      value: false,
    },
  ],
  [
    "discoverystream.region-basic-layout",
    {
      title: "Decision to use basic layout based on region.",
      getValue: ({ geo }) => {
        const preffedRegionsString =
          Services.prefs.getStringPref(REGION_LAYOUT_CONFIG) || "";
        const preffedRegions = preffedRegionsString
          .split(",")
          .map(s => s.trim());

        return !preffedRegions.includes(geo);
      },
    },
  ],
  [
    "discoverystream.spoc.impressions",
    {
      title: "Track spoc impressions",
      skipBroadcast: true,
      value: "{}",
    },
  ],
  [
    "discoverystream.endpointSpocsClear",
    {
      title:
        "Endpoint for when a user opts-out of sponsored content to delete the user's data from the ad server.",
      value: "https://spocs.getpocket.com/user",
    },
  ],
  [
    "discoverystream.rec.impressions",
    {
      title: "Track rec impressions",
      skipBroadcast: true,
      value: "{}",
    },
  ],
]);

// Array of each feed's FEEDS_CONFIG factory and values to add to PREFS_CONFIG
const FEEDS_DATA = [
  {
    name: "aboutpreferences",
    factory: () => new AboutPreferences(),
    title: "about:preferences rendering",
    value: true,
  },
  {
    name: "newtabinit",
    factory: () => new NewTabInit(),
    title: "Sends a copy of the state to each new tab that is opened",
    value: true,
  },
  {
    name: "places",
    factory: () => new PlacesFeed(),
    title: "Listens for and relays various Places-related events",
    value: true,
  },
  {
    name: "prefs",
    factory: () => new PrefsFeed(PREFS_CONFIG),
    title: "Preferences",
    value: true,
  },
  {
    name: "sections",
    factory: () => new SectionsFeed(),
    title: "Manages sections",
    value: true,
  },
  {
    name: "section.highlights",
    factory: () => new HighlightsFeed(),
    title: "Fetches content recommendations from places db",
    value: true,
  },
  {
    name: "section.topstories",
    factory: () =>
      new TopStoriesFeed(PREFS_CONFIG.get("discoverystream.config")),
    title:
      "Fetches content recommendations from a configurable content provider",
    // Dynamically determine if Pocket should be shown for a geo / locale
    getValue: ({ geo, locale }) => {
      const preffedRegionsString =
        Services.prefs.getStringPref(REGION_STORIES_CONFIG) || "";
      const preffedRegions = preffedRegionsString.split(",").map(s => s.trim());
      const locales = {
        US: ["en-CA", "en-GB", "en-US"],
        CA: ["en-CA", "en-GB", "en-US"],
        GB: ["en-CA", "en-GB", "en-US"],
        AU: ["en-CA", "en-GB", "en-US"],
        NZ: ["en-CA", "en-GB", "en-US"],
        IN: ["en-CA", "en-GB", "en-US"],
        ZA: ["en-CA", "en-GB", "en-US"],
        CH: ["de", "fr", "it"],
        BE: ["fr", "de"],
        DE: ["de"],
        AT: ["de"],
        IT: ["it"],
        FR: ["fr"],
        ES: ["es"],
        PL: ["pl"],
        JP: ["ja", "ja-JP-mac"],
      }[geo];
      return (
        preffedRegions.includes(geo) && !!locales && locales.includes(locale)
      );
    },
  },
  {
    name: "systemtick",
    factory: () => new SystemTickFeed(),
    title: "Produces system tick events to periodically check for data expiry",
    value: true,
  },
  {
    name: "telemetry",
    factory: () => new TelemetryFeed(),
    title: "Relays telemetry-related actions to PingCentre",
    value: true,
  },
  {
    name: "favicon",
    factory: () => new FaviconFeed(),
    title: "Fetches tippy top manifests from remote service",
    value: true,
  },
  {
    name: "topsites",
    factory: () => new TopSitesFeed(),
    title: "Queries places and gets metadata for Top Sites section",
    value: true,
  },
  {
    name: "asrouterfeed",
    factory: () => new ASRouterFeed(),
    title: "Handles AS Router messages, such as snippets and onboaridng",
    value: true,
  },
  {
    name: "recommendationproviderswitcher",
    factory: () => new RecommendationProviderSwitcher(),
    title: "Handles switching between two types of personality providers",
    value: true,
  },
  {
    name: "discoverystreamfeed",
    factory: () => new DiscoveryStreamFeed(),
    title: "Handles new pocket ui for the new tab page",
    value: true,
  },
];

const FEEDS_CONFIG = new Map();
for (const config of FEEDS_DATA) {
  const pref = `feeds.${config.name}`;
  FEEDS_CONFIG.set(pref, config.factory);
  PREFS_CONFIG.set(pref, config);
}

this.ActivityStream = class ActivityStream {
  /**
   * constructor - Initializes an instance of ActivityStream
   */
  constructor() {
    this.initialized = false;
    this.store = new Store();
    this.feeds = FEEDS_CONFIG;
    this._defaultPrefs = new DefaultPrefs(PREFS_CONFIG);
  }

  init() {
    try {
      this._updateDynamicPrefs();
      this._defaultPrefs.init();

      // Look for outdated user pref values that might have been accidentally
      // persisted when restoring the original pref value at the end of an
      // experiment across versions with a different default value.
      const DS_CONFIG =
        "browser.newtabpage.activity-stream.discoverystream.config";
      if (
        Services.prefs.prefHasUserValue(DS_CONFIG) &&
        [
          // Firefox 66
          `{"api_key_pref":"extensions.pocket.oAuthConsumerKey","enabled":false,"show_spocs":true,"layout_endpoint":"https://getpocket.com/v3/newtab/layout?version=1&consumer_key=$apiKey&layout_variant=basic"}`,
          // Firefox 67
          `{"api_key_pref":"extensions.pocket.oAuthConsumerKey","enabled":false,"show_spocs":true,"layout_endpoint":"https://getpocket.cdn.mozilla.net/v3/newtab/layout?version=1&consumer_key=$apiKey&layout_variant=basic"}`,
          // Firefox 68
          `{"api_key_pref":"extensions.pocket.oAuthConsumerKey","collapsible":true,"enabled":false,"show_spocs":true,"hardcoded_layout":true,"personalized":false,"layout_endpoint":"https://getpocket.cdn.mozilla.net/v3/newtab/layout?version=1&consumer_key=$apiKey&layout_variant=basic"}`,
        ].includes(Services.prefs.getStringPref(DS_CONFIG))
      ) {
        Services.prefs.clearUserPref(DS_CONFIG);
      }

      // Hook up the store and let all feeds and pages initialize
      this.store.init(
        this.feeds,
        ac.BroadcastToContent({
          type: at.INIT,
          data: {
            locale: this.locale,
          },
        }),
        { type: at.UNINIT }
      );

      this.initialized = true;
    } catch (e) {
      // TelemetryFeed could be unavailable if the telemetry is disabled, or
      // the telemetry feed is not yet initialized.
      const telemetryFeed = this.store.feeds.get("feeds.telemetry");
      if (telemetryFeed) {
        telemetryFeed.handleUndesiredEvent({
          data: { event: "ADDON_INIT_FAILED" },
        });
      }
      throw e;
    }
  }

  /**
   * Check if an old pref has a custom value to migrate. Clears the pref so that
   * it's the default after migrating (to avoid future need to migrate).
   *
   * @param oldPrefName {string} Pref to check and migrate
   * @param cbIfNotDefault {function} Callback that gets the current pref value
   */
  _migratePref(oldPrefName, cbIfNotDefault) {
    // Nothing to do if the user doesn't have a custom value
    if (!Services.prefs.prefHasUserValue(oldPrefName)) {
      return;
    }

    // Figure out what kind of pref getter to use
    let prefGetter;
    switch (Services.prefs.getPrefType(oldPrefName)) {
      case Services.prefs.PREF_BOOL:
        prefGetter = "getBoolPref";
        break;
      case Services.prefs.PREF_INT:
        prefGetter = "getIntPref";
        break;
      case Services.prefs.PREF_STRING:
        prefGetter = "getStringPref";
        break;
    }

    // Give the callback the current value then clear the pref
    cbIfNotDefault(Services.prefs[prefGetter](oldPrefName));
    Services.prefs.clearUserPref(oldPrefName);
  }

  uninit() {
    if (this.geo === "") {
      Services.prefs.removeObserver(GEO_PREF, this);
    }

    this.store.uninit();
    this.initialized = false;
  }

  _updateDynamicPrefs() {
    // Save the geo pref if we have it
    if (Services.prefs.prefHasUserValue(GEO_PREF)) {
      this.geo = Services.prefs.getStringPref(GEO_PREF);
    } else if (this.geo !== "") {
      // Watch for geo changes and use a dummy value for now
      Services.prefs.addObserver(GEO_PREF, this);
      this.geo = "";
    }

    this.locale = Services.locale.appLocaleAsBCP47;

    // Update the pref config of those with dynamic values
    for (const pref of PREFS_CONFIG.keys()) {
      // Only need to process dynamic prefs
      const prefConfig = PREFS_CONFIG.get(pref);
      if (!prefConfig.getValue) {
        continue;
      }

      // Have the dynamic pref just reuse using existing default, e.g., those
      // set via Autoconfig or policy
      try {
        const existingDefault = this._defaultPrefs.get(pref);
        if (existingDefault !== undefined && prefConfig.value === undefined) {
          prefConfig.getValue = () => existingDefault;
        }
      } catch (ex) {
        // We get NS_ERROR_UNEXPECTED for prefs that have a user value (causing
        // default branch to believe there's a type) but no actual default value
      }

      // Compute the dynamic value (potentially generic based on dummy geo)
      const newValue = prefConfig.getValue({
        geo: this.geo,
        locale: this.locale,
      });

      // If there's an existing value and it has changed, that means we need to
      // overwrite the default with the new value.
      if (prefConfig.value !== undefined && prefConfig.value !== newValue) {
        this._defaultPrefs.set(pref, newValue);
      }

      prefConfig.value = newValue;
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        // We should only expect one geo change, so update and stop observing
        if (data === GEO_PREF) {
          this._updateDynamicPrefs();
          Services.prefs.removeObserver(GEO_PREF, this);
        }
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["ActivityStream", "PREFS_CONFIG"];
