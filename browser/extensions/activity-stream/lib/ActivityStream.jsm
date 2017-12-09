/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

// NB: Eagerly load modules that will be loaded/constructed/initialized in the
// common case to avoid the overhead of wrapping and detecting lazy loading.
const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {DefaultPrefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {ManualMigration} = Cu.import("resource://activity-stream/lib/ManualMigration.jsm", {});
const {NewTabInit} = Cu.import("resource://activity-stream/lib/NewTabInit.jsm", {});
const {SectionsFeed} = Cu.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {PlacesFeed} = Cu.import("resource://activity-stream/lib/PlacesFeed.jsm", {});
const {PrefsFeed} = Cu.import("resource://activity-stream/lib/PrefsFeed.jsm", {});
const {Store} = Cu.import("resource://activity-stream/lib/Store.jsm", {});
const {SnippetsFeed} = Cu.import("resource://activity-stream/lib/SnippetsFeed.jsm", {});
const {SystemTickFeed} = Cu.import("resource://activity-stream/lib/SystemTickFeed.jsm", {});
const {TelemetryFeed} = Cu.import("resource://activity-stream/lib/TelemetryFeed.jsm", {});
const {FaviconFeed} = Cu.import("resource://activity-stream/lib/FaviconFeed.jsm", {});
const {TopSitesFeed} = Cu.import("resource://activity-stream/lib/TopSitesFeed.jsm", {});
const {TopStoriesFeed} = Cu.import("resource://activity-stream/lib/TopStoriesFeed.jsm", {});
const {HighlightsFeed} = Cu.import("resource://activity-stream/lib/HighlightsFeed.jsm", {});

const DEFAULT_SITES = new Map([
  // This first item is the global list fallback for any unexpected geos
  ["", "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.reddit.com/,https://www.amazon.com/,https://twitter.com/"],
  ["US", "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/"],
  ["CA", "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://www.amazon.ca/,https://twitter.com/"],
  ["DE", "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.de/,https://www.ebay.de/,https://www.wikipedia.org/,https://www.reddit.com/"],
  ["PL", "https://www.youtube.com/,https://www.facebook.com/,https://allegro.pl/,https://www.wikipedia.org/,https://www.olx.pl/,https://www.wykop.pl/"],
  ["RU", "https://vk.com/,https://www.youtube.com/,https://ok.ru/,https://www.avito.ru/,https://www.aliexpress.com/,https://www.wikipedia.org/"],
  ["GB", "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.amazon.co.uk/,https://www.bbc.co.uk/,https://www.ebay.co.uk/"],
  ["FR", "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.amazon.fr/,https://www.leboncoin.fr/,https://twitter.com/"]
]);
const GEO_PREF = "browser.search.region";
const REASON_ADDON_UNINSTALL = 6;

// Configure default Activity Stream prefs with a plain `value` or a `getValue`
// that computes a value. A `value_local_dev` is used for development defaults.
const PREFS_CONFIG = new Map([
  ["default.sites", {
    title: "Comma-separated list of default top sites to fill in behind visited sites",
    getValue: ({geo}) => DEFAULT_SITES.get(DEFAULT_SITES.has(geo) ? geo : "")
  }],
  ["feeds.section.topstories.options", {
    title: "Configuration options for top stories feed",
    // This is a dynamic pref as it depends on the feed being shown or not
    getValue: args => JSON.stringify({
      api_key_pref: "extensions.pocket.oAuthConsumerKey",
      // Use the opposite value as what default value the feed would have used
      hidden: !PREFS_CONFIG.get("feeds.section.topstories").getValue(args),
      provider_header: "pocket_feedback_header",
      provider_description: "pocket_description",
      provider_icon: "pocket",
      provider_name: "Pocket",
      read_more_endpoint: "https://getpocket.com/explore/trending?src=fx_new_tab",
      stories_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=2&consumer_key=$apiKey&locale_lang=${args.locale}`,
      stories_referrer: "http://getpocket.com/recommendations",
      info_link: "https://www.mozilla.org/privacy/firefox/#pocketstories",
      disclaimer_link: "https://getpocket.cdn.mozilla.net/firefox/new_tab_learn_more",
      topics_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/trending-topics?version=2&consumer_key=$apiKey&locale_lang=${args.locale}`,
      show_spocs: false,
      personalized: true
    })
  }],
  ["showSponsored", {
    title: "Show sponsored cards in spoc experiment (show_spocs in topstories.options has to be set to true as well)",
    value: true
  }],
  ["filterAdult", {
    title: "Remove adult pages from sites, highlights, etc.",
    value: true
  }],
  ["migrationExpired", {
    title: "Boolean flag that decides whether to show the migration message or not.",
    value: false
  }],
  ["migrationLastShownDate", {
    title: "Timestamp when migration message was last shown. In seconds.",
    value: 0
  }],
  ["migrationRemainingDays", {
    title: "Number of days to show the manual migration message",
    value: 4
  }],
  ["prerender", {
    title: "Use the prerendered version of activity-stream.html. This is set automatically by PrefsFeed.jsm.",
    value: true
  }],
  ["showSearch", {
    title: "Show the Search bar",
    value: true
  }],
  ["disableSnippets", {
    title: "Disable snippets on activity stream",
    value: false
  }],
  ["showTopSites", {
    title: "Show the Top Sites section",
    value: true
  }],
  ["collapseTopSites", {
    title: "Collapse the Top Sites section",
    value: false
  }],
  ["topSitesCount", {
    title: "Number of Top Sites to display",
    value: 6
  }],
  ["telemetry", {
    title: "Enable system error and usage data collection",
    value: true,
    value_local_dev: false
  }],
  ["telemetry.ping.endpoint", {
    title: "Telemetry server endpoint",
    value: "https://tiles.services.mozilla.com/v4/links/activity-stream"
  }],
  ["section.highlights.collapsed", {
    title: "Collapse the Highlights section",
    value: false
  }],
  ["section.topstories.collapsed", {
    title: "Collapse the Top Stories section",
    value: false
  }],
  ["section.topstories.showDisclaimer", {
    title: "Boolean flag that decides whether or not to show the topstories disclaimer.",
    value: true
  }],
  ["tippyTop.service.endpoint", {
    title: "Tippy Top service manifest url",
    value: "https://activity-stream-icons.services.mozilla.com/v1/icons.json.br"
  }]
]);

// Array of each feed's FEEDS_CONFIG factory and values to add to PREFS_CONFIG
const FEEDS_DATA = [
  {
    name: "migration",
    factory: () => new ManualMigration(),
    title: "Manual migration wizard",
    value: true
  },
  {
    name: "newtabinit",
    factory: () => new NewTabInit(),
    title: "Sends a copy of the state to each new tab that is opened",
    value: true
  },
  {
    name: "places",
    factory: () => new PlacesFeed(),
    title: "Listens for and relays various Places-related events",
    value: true
  },
  {
    name: "prefs",
    factory: () => new PrefsFeed(PREFS_CONFIG),
    title: "Preferences",
    value: true
  },
  {
    name: "sections",
    factory: () => new SectionsFeed(),
    title: "Manages sections",
    value: true
  },
  {
    name: "section.highlights",
    factory: () => new HighlightsFeed(),
    title: "Fetches content recommendations from places db",
    value: true
  },
  {
    name: "section.topstories",
    factory: () => new TopStoriesFeed(),
    title: "Fetches content recommendations from a configurable content provider",
    // Dynamically determine if Pocket should be shown for a geo / locale
    getValue: ({geo, locale}) => {
      const locales = ({
        "US": ["en-US", "en-GB", "en-ZA"],
        "CA": ["en-US", "en-GB", "en-ZA"],
        "DE": ["de", "de-DE", "de-AT", "de-CH"]
      })[geo];
      return !!locales && locales.includes(locale);
    }
  },
  {
    name: "snippets",
    factory: () => new SnippetsFeed(),
    title: "Gets snippets data",
    value: true
  },
  {
    name: "systemtick",
    factory: () => new SystemTickFeed(),
    title: "Produces system tick events to periodically check for data expiry",
    value: true
  },
  {
    name: "telemetry",
    factory: () => new TelemetryFeed(),
    title: "Relays telemetry-related actions to PingCentre",
    value: true
  },
  {
    name: "favicon",
    factory: () => new FaviconFeed(),
    title: "Fetches tippy top manifests from remote service",
    value: true
  },
  {
    name: "topsites",
    factory: () => new TopSitesFeed(),
    title: "Queries places and gets metadata for Top Sites section",
    value: true
  }
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
   *
   * @param  {object} options Options for the ActivityStream instance
   * @param  {string} options.id Add-on ID. e.g. "activity-stream@mozilla.org".
   * @param  {string} options.version Version of the add-on. e.g. "0.1.0"
   * @param  {string} options.newTabURL URL of New Tab page on which A.S. is displayed. e.g. "about:newtab"
   */
  constructor(options = {}) {
    this.initialized = false;
    this.options = options;
    this.store = new Store();
    this.feeds = FEEDS_CONFIG;
    this._defaultPrefs = new DefaultPrefs(PREFS_CONFIG);
  }
  init() {
    try {
      this._updateDynamicPrefs();
      this._defaultPrefs.init();

      // Hook up the store and let all feeds and pages initialize
      this.store.init(this.feeds, ac.BroadcastToContent({
        type: at.INIT,
        data: {version: this.options.version}
      }), {type: at.UNINIT});

      this.initialized = true;
    } catch (e) {
      // TelemetryFeed could be unavailable if the telemetry is disabled, or
      // the telemetry feed is not yet initialized.
      const telemetryFeed = this.store.feeds.get("feeds.telemetry");
      if (telemetryFeed) {
        telemetryFeed.handleUndesiredEvent({data: {event: "ADDON_INIT_FAILED"}});
      }
      throw e;
    }
  }
  uninit() {
    if (this.geo === "") {
      Services.prefs.removeObserver(GEO_PREF, this);
    }

    this.store.uninit();
    this.initialized = false;
  }
  uninstall(reason) {
    if (reason === REASON_ADDON_UNINSTALL) {
      // This resets all prefs in the config to their default values,
      // so we DON'T want to do this on an upgrade/downgrade, only on a
      // real uninstall
      this._defaultPrefs.reset();
    }
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

    this.locale = Services.locale.getRequestedLocale();

    // Update the pref config of those with dynamic values
    for (const pref of PREFS_CONFIG.keys()) {
      const prefConfig = PREFS_CONFIG.get(pref);
      if (!prefConfig.getValue) {
        continue;
      }

      const newValue = prefConfig.getValue({
        geo: this.geo,
        locale: this.locale
      });

      // If there's an existing value and it has changed, that means we need to
      // overwrite the default with the new value.
      if (prefConfig.value !== undefined && prefConfig.value !== newValue) {
        this._defaultPrefs.setDefaultPref(pref, newValue);
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

this.EXPORTED_SYMBOLS = ["ActivityStream", "PREFS_CONFIG"];
