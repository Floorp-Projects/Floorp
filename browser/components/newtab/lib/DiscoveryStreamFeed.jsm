/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Region",
  "resource://gre/modules/Region.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PersistentCache",
  "resource://activity-stream/lib/PersistentCache.jsm"
);
XPCOMUtils.defineLazyServiceGetters(this, {
  gUUIDGenerator: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

const CACHE_KEY = "discovery_stream";
const LAYOUT_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const STARTUP_CACHE_EXPIRE_TIME = 7 * 24 * 60 * 60 * 1000; // 1 week
const COMPONENT_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const SPOCS_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const DEFAULT_RECS_EXPIRE_TIME = 60 * 60 * 1000; // 1 hour
const MIN_DOMAIN_AFFINITIES_UPDATE_TIME = 12 * 60 * 60 * 1000; // 12 hours
const MAX_LIFETIME_CAP = 500; // Guard against misconfiguration on the server
const DEFAULT_MAX_HISTORY_QUERY_RESULTS = 1000;
const FETCH_TIMEOUT = 45 * 1000;
const PREF_CONFIG = "discoverystream.config";
const PREF_ENDPOINTS = "discoverystream.endpoints";
const PREF_IMPRESSION_ID = "browser.newtabpage.activity-stream.impressionId";
const PREF_ENABLED = "discoverystream.enabled";
const PREF_HARDCODED_BASIC_LAYOUT = "discoverystream.hardcoded-basic-layout";
const PREF_SPOCS_ENDPOINT = "discoverystream.spocs-endpoint";
const PREF_SPOCS_ENDPOINT_QUERY = "discoverystream.spocs-endpoint-query";
const PREF_REGION_BASIC_LAYOUT = "discoverystream.region-basic-layout";
const PREF_USER_TOPSTORIES = "feeds.section.topstories";
const PREF_SYSTEM_TOPSTORIES = "feeds.system.topstories";
const PREF_SPOCS_CLEAR_ENDPOINT = "discoverystream.endpointSpocsClear";
const PREF_SHOW_SPONSORED = "showSponsored";
const PREF_SPOC_IMPRESSIONS = "discoverystream.spoc.impressions";
const PREF_FLIGHT_BLOCKS = "discoverystream.flight.blocks";
const PREF_REC_IMPRESSIONS = "discoverystream.rec.impressions";
const PREF_COLLECTION_DISMISSIBLE = "discoverystream.isCollectionDismissible";
const PREF_RECS_PERSONALIZED = "discoverystream.recs.personalized";
const PREF_SPOCS_PERSONALIZED = "discoverystream.spocs.personalized";
const PREF_PERSONALIZATION_VERSION = "discoverystream.personalization.version";
const PREF_PERSONALIZATION_OVERRIDE_VERSION =
  "discoverystream.personalization.overrideVersion";

let getHardcodedLayout;

this.DiscoveryStreamFeed = class DiscoveryStreamFeed {
  constructor() {
    // Internal state for checking if we've intialized all our data
    this.loaded = false;

    // Persistent cache for remote endpoint data.
    this.cache = new PersistentCache(CACHE_KEY, true);
    this.locale = Services.locale.appLocaleAsBCP47;
    this._impressionId = this.getOrCreateImpressionId();
    // Internal in-memory cache for parsing json prefs.
    this._prefCache = {};
  }

  getOrCreateImpressionId() {
    let impressionId = Services.prefs.getCharPref(PREF_IMPRESSION_ID, "");
    if (!impressionId) {
      impressionId = String(gUUIDGenerator.generateUUID());
      Services.prefs.setCharPref(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  }

  finalLayoutEndpoint(url, apiKey) {
    if (url.includes("$apiKey") && !apiKey) {
      throw new Error(
        `Layout Endpoint - An API key was specified but none configured: ${url}`
      );
    }
    return url.replace("$apiKey", apiKey);
  }

  get config() {
    if (this._prefCache.config) {
      return this._prefCache.config;
    }
    try {
      this._prefCache.config = JSON.parse(
        this.store.getState().Prefs.values[PREF_CONFIG]
      );
      const layoutUrl = this._prefCache.config.layout_endpoint;

      const apiKeyPref = this._prefCache.config.api_key_pref;
      if (layoutUrl && apiKeyPref) {
        const apiKey = Services.prefs.getCharPref(apiKeyPref, "");
        this._prefCache.config.layout_endpoint = this.finalLayoutEndpoint(
          layoutUrl,
          apiKey
        );
      }
    } catch (e) {
      // istanbul ignore next
      this._prefCache.config = {};
      // istanbul ignore next
      Cu.reportError(
        `Could not parse preference. Try resetting ${PREF_CONFIG} in about:config. ${e}`
      );
    }
    this._prefCache.config.enabled =
      this._prefCache.config.enabled &&
      this.store.getState().Prefs.values[PREF_ENABLED];

    return this._prefCache.config;
  }

  resetConfigDefauts() {
    this.store.dispatch({
      type: at.CLEAR_PREF,
      data: {
        name: PREF_CONFIG,
      },
    });
  }

  get region() {
    return Region.home;
  }

  get showSpocs() {
    // Combine user-set sponsored opt-out with Mozilla-set config
    return (
      this.store.getState().Prefs.values[PREF_SHOW_SPONSORED] &&
      this.config.show_spocs
    );
  }

  get showStories() {
    // Combine user-set sponsored opt-out with Mozilla-set config
    return (
      this.store.getState().Prefs.values[PREF_SYSTEM_TOPSTORIES] &&
      this.store.getState().Prefs.values[PREF_USER_TOPSTORIES]
    );
  }

  get personalized() {
    // If both spocs and recs are not personalized, we might as well return false here.
    const spocsPersonalized = this.store.getState().Prefs.values[
      PREF_SPOCS_PERSONALIZED
    ];
    const recsPersonalized = this.store.getState().Prefs.values[
      PREF_RECS_PERSONALIZED
    ];

    return (
      this.config.personalized &&
      !!this.providerSwitcher &&
      (spocsPersonalized || recsPersonalized)
    );
  }

  get providerSwitcher() {
    if (this._providerSwitcher) {
      return this._providerSwitcher;
    }
    this._providerSwitcher = this.store.feeds.get(
      "feeds.recommendationproviderswitcher"
    );
    return this._providerSwitcher;
  }

  setupPrefs(isStartup = false) {
    // Send the initial state of the pref on our reducer
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_CONFIG_SETUP,
        data: this.config,
        meta: {
          isStartup,
        },
      })
    );
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE,
        data: {
          value: this.store.getState().Prefs.values[
            PREF_COLLECTION_DISMISSIBLE
          ],
        },
        meta: {
          isStartup,
        },
      })
    );
  }

  uninitPrefs() {
    // Reset in-memory cache
    this._prefCache = {};
  }

  async fetchFromEndpoint(rawEndpoint, options = {}) {
    if (!rawEndpoint) {
      Cu.reportError("Tried to fetch endpoint but none was configured.");
      return null;
    }

    const apiKeyPref = this._prefCache.config.api_key_pref;
    const apiKey = Services.prefs.getCharPref(apiKeyPref, "");

    // The server somtimes returns this value already replaced, but we try this for two reasons:
    // 1. Layout endpoints are not from the server.
    // 2. Hardcoded layouts don't have this already done for us.
    const endpoint = rawEndpoint
      .replace("$apiKey", apiKey)
      .replace("$locale", this.locale)
      .replace("$region", this.region);

    try {
      // Make sure the requested endpoint is allowed
      const allowed = this.store
        .getState()
        .Prefs.values[PREF_ENDPOINTS].split(",");
      if (!allowed.some(prefix => endpoint.startsWith(prefix))) {
        throw new Error(`Not one of allowed prefixes (${allowed})`);
      }

      const controller = new AbortController();
      const { signal } = controller;

      const fetchPromise = fetch(endpoint, {
        ...options,
        credentials: "omit",
        signal,
      });
      // istanbul ignore next
      const timeoutId = setTimeout(() => {
        controller.abort();
      }, FETCH_TIMEOUT);

      const response = await fetchPromise;
      if (!response.ok) {
        throw new Error(`Unexpected status (${response.status})`);
      }
      clearTimeout(timeoutId);
      return response.json();
    } catch (error) {
      Cu.reportError(`Failed to fetch ${endpoint}: ${error.message}`);
    }
    return null;
  }

  /**
   * Returns true if data in the cache for a particular key has expired or is missing.
   * @param {object} cachedData data returned from cache.get()
   * @param {string} key a cache key
   * @param {string?} url for "feed" only, the URL of the feed.
   * @param {boolean} is this check done at initial browser load
   */
  isExpired({ cachedData, key, url, isStartup }) {
    const { layout, spocs, feeds } = cachedData;
    const updateTimePerComponent = {
      layout: LAYOUT_UPDATE_TIME,
      spocs: SPOCS_FEEDS_UPDATE_TIME,
      feed: COMPONENT_FEEDS_UPDATE_TIME,
    };
    const EXPIRATION_TIME = isStartup
      ? STARTUP_CACHE_EXPIRE_TIME
      : updateTimePerComponent[key];
    switch (key) {
      case "layout":
        // This never needs to expire, as it's not expected to change.
        if (this.config.hardcoded_layout) {
          return false;
        }
        return !layout || !(Date.now() - layout.lastUpdated < EXPIRATION_TIME);
      case "spocs":
        return !spocs || !(Date.now() - spocs.lastUpdated < EXPIRATION_TIME);
      case "feed":
        return (
          !feeds ||
          !feeds[url] ||
          !(Date.now() - feeds[url].lastUpdated < EXPIRATION_TIME)
        );
      default:
        // istanbul ignore next
        throw new Error(`${key} is not a valid key`);
    }
  }

  async _checkExpirationPerComponent() {
    const cachedData = (await this.cache.get()) || {};
    const { feeds } = cachedData;
    return {
      layout: this.isExpired({ cachedData, key: "layout" }),
      spocs: this.isExpired({ cachedData, key: "spocs" }),
      feeds:
        !feeds ||
        Object.keys(feeds).some(url =>
          this.isExpired({ cachedData, key: "feed", url })
        ),
    };
  }

  /**
   * Returns true if any data for the cached endpoints has expired or is missing.
   */
  async checkIfAnyCacheExpired() {
    const expirationPerComponent = await this._checkExpirationPerComponent();
    return (
      expirationPerComponent.layout ||
      expirationPerComponent.spocs ||
      expirationPerComponent.feeds
    );
  }

  async fetchLayout(isStartup) {
    const cachedData = (await this.cache.get()) || {};
    let { layout } = cachedData;
    if (this.isExpired({ cachedData, key: "layout", isStartup })) {
      const layoutResponse = await this.fetchFromEndpoint(
        this.config.layout_endpoint
      );
      if (layoutResponse && layoutResponse.layout) {
        layout = {
          lastUpdated: Date.now(),
          spocs: layoutResponse.spocs,
          layout: layoutResponse.layout,
          status: "success",
        };

        await this.cache.set("layout", layout);
      } else {
        Cu.reportError("No response for response.layout prop");
      }
    }
    return layout;
  }

  updatePlacements(sendUpdate, layout, isStartup = false) {
    const placements = [];
    const placementsMap = {};
    for (const row of layout.filter(r => r.components && r.components.length)) {
      for (const component of row.components) {
        if (component.placement) {
          // Throw away any dupes for the request.
          if (!placementsMap[component.placement.name]) {
            placementsMap[component.placement.name] = component.placement;
            placements.push(component.placement);
          }
        }
      }
    }
    if (placements.length) {
      sendUpdate({
        type: at.DISCOVERY_STREAM_SPOCS_PLACEMENTS,
        data: { placements },
        meta: {
          isStartup,
        },
      });
    }
  }

  /**
   * Adds a query string to a URL.
   * A query can be any string literal accepted by https://developer.mozilla.org/docs/Web/API/URLSearchParams
   * Examples: "?foo=1&bar=2", "&foo=1&bar=2", "foo=1&bar=2", "?bar=2" or "bar=2"
   */
  addEndpointQuery(url, query) {
    if (!query) {
      return url;
    }

    const urlObject = new URL(url);
    const params = new URLSearchParams(query);

    for (let [key, val] of params.entries()) {
      urlObject.searchParams.append(key, val);
    }

    return urlObject.toString();
  }

  async loadLayout(sendUpdate, isStartup) {
    let layoutResp = {};
    let url = "";
    if (!this.config.hardcoded_layout) {
      layoutResp = await this.fetchLayout(isStartup);
    }

    if (!layoutResp || !layoutResp.layout) {
      const isBasic =
        this.config.hardcoded_basic_layout ||
        this.store.getState().Prefs.values[PREF_HARDCODED_BASIC_LAYOUT] ||
        this.store.getState().Prefs.values[PREF_REGION_BASIC_LAYOUT];

      // Set a hardcoded layout if one is needed.
      // Changing values in this layout in memory object is unnecessary.
      layoutResp = getHardcodedLayout(isBasic);
    }

    sendUpdate({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: layoutResp,
      meta: {
        isStartup,
      },
    });

    if (layoutResp.spocs) {
      url =
        this.store.getState().Prefs.values[PREF_SPOCS_ENDPOINT] ||
        this.config.spocs_endpoint ||
        layoutResp.spocs.url;

      const spocsEndpointQuery = this.store.getState().Prefs.values[
        PREF_SPOCS_ENDPOINT_QUERY
      ];

      // For QA, testing, or debugging purposes, there may be a query string to add.
      url = this.addEndpointQuery(url, spocsEndpointQuery);

      if (
        url &&
        url !== this.store.getState().DiscoveryStream.spocs.spocs_endpoint
      ) {
        sendUpdate({
          type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
          data: {
            url,
          },
          meta: {
            isStartup,
          },
        });
        this.updatePlacements(sendUpdate, layoutResp.layout, isStartup);
      }
    }
  }

  /**
   * buildFeedPromise - Adds the promise result to newFeeds and
   *                    pushes a promise to newsFeedsPromises.
   * @param {Object} Has both newFeedsPromises (Array) and newFeeds (Object)
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Function} We return a function so we can contain
   *                     the scope for isStartup and the promises object.
   *                     Combines feed results and promises for each component with a feed.
   */
  buildFeedPromise(
    { newFeedsPromises, newFeeds },
    isStartup = false,
    sendUpdate
  ) {
    return component => {
      const { url } = component.feed;

      if (!newFeeds[url]) {
        // We initially stub this out so we don't fetch dupes,
        // we then fill in with the proper object inside the promise.
        newFeeds[url] = {};
        const feedPromise = this.getComponentFeed(url, isStartup);

        feedPromise
          .then(feed => {
            // If we stored the result of filter in feed cache as it happened,
            // I think we could reduce doing this for cache fetches.
            // Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1606277
            newFeeds[url] = this.filterRecommendations(feed);
            sendUpdate({
              type: at.DISCOVERY_STREAM_FEED_UPDATE,
              data: {
                feed: newFeeds[url],
                url,
              },
              meta: {
                isStartup,
              },
            });

            // We grab affinities off the first feed for the moment.
            // Ideally this would be returned from the server on the layout,
            // or from another endpoint.
            if (!this.affinities) {
              const { settings } = feed.data;
              this.affinities = {
                timeSegments: settings.timeSegments,
                parameterSets: settings.domainAffinityParameterSets,
                maxHistoryQueryResults:
                  settings.maxHistoryQueryResults ||
                  DEFAULT_MAX_HISTORY_QUERY_RESULTS,
                version: settings.version,
              };
            }
          })
          .catch(
            /* istanbul ignore next */ error => {
              Cu.reportError(
                `Error trying to load component feed ${url}: ${error}`
              );
            }
          );
        newFeedsPromises.push(feedPromise);
      }
    };
  }

  filterRecommendations(feed) {
    if (
      feed &&
      feed.data &&
      feed.data.recommendations &&
      feed.data.recommendations.length
    ) {
      const { data: recommendations } = this.filterBlocked(
        feed.data.recommendations
      );
      return {
        ...feed,
        data: {
          ...feed.data,
          recommendations,
        },
      };
    }
    return feed;
  }

  /**
   * reduceFeedComponents - Filters out components with no feeds, and combines
   *                        all feeds on this component with the feeds from other components.
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Function} We return a function so we can contain the scope for isStartup.
   *                     Reduces feeds into promises and feed data.
   */
  reduceFeedComponents(isStartup, sendUpdate) {
    return (accumulator, row) => {
      row.components
        .filter(component => component && component.feed)
        .forEach(this.buildFeedPromise(accumulator, isStartup, sendUpdate));
      return accumulator;
    };
  }

  /**
   * buildFeedPromises - Filters out rows with no components,
   *                     and gets us a promise for each unique feed.
   * @param {Object} layout This is the Discovery Stream layout object.
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Object} An object with newFeedsPromises (Array) and newFeeds (Object),
   *                   we can Promise.all newFeedsPromises to get completed data in newFeeds.
   */
  buildFeedPromises(layout, isStartup, sendUpdate) {
    const initialData = {
      newFeedsPromises: [],
      newFeeds: {},
    };
    return layout
      .filter(row => row && row.components)
      .reduce(this.reduceFeedComponents(isStartup, sendUpdate), initialData);
  }

  async loadComponentFeeds(sendUpdate, isStartup = false) {
    const { DiscoveryStream } = this.store.getState();

    if (!DiscoveryStream || !DiscoveryStream.layout) {
      return;
    }

    // Reset the flag that indicates whether or not at least one API request
    // was issued to fetch the component feed in `getComponentFeed()`.
    this.componentFeedFetched = false;
    const { newFeedsPromises, newFeeds } = this.buildFeedPromises(
      DiscoveryStream.layout,
      isStartup,
      sendUpdate
    );

    // Each promise has a catch already built in, so no need to catch here.
    await Promise.all(newFeedsPromises);

    if (this.componentFeedFetched) {
      this.cleanUpTopRecImpressionPref(newFeeds);
    }
    await this.cache.set("feeds", newFeeds);
    sendUpdate({
      type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
      meta: {
        isStartup,
      },
    });
  }

  getPlacements() {
    const { placements } = this.store.getState().DiscoveryStream.spocs;
    // Backwards comp for before we had placements, assume just a single spocs placement.
    if (!placements || !placements.length) {
      return [{ name: "spocs" }];
    }
    return placements;
  }

  // I wonder, can this be better as a reducer?
  // See Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1606717
  placementsForEach(callback) {
    this.getPlacements().forEach(callback);
  }

  // Bug 1567271 introduced meta data on a list of spocs.
  // This involved moving the spocs array into an items prop.
  // However, old data could still be returned, and cached data might also be old.
  // For ths reason, we want to ensure if we don't find an items array,
  // we use the previous array placement, and then stub out title and context to empty strings.
  // We need to do this *after* both fresh fetches and cached data to reduce repetition.
  normalizeSpocsItems(spocs) {
    const items = spocs.items || spocs;
    const title = spocs.title || "";
    const context = spocs.context || "";
    const sponsor = spocs.sponsor || "";
    // We do not stub sponsored_by_override with an empty string. It is an override, and an empty string
    // explicitly means to override the client to display an empty string.
    // An empty string is not an no op in this case. Undefined is the proper no op here.
    const { sponsored_by_override } = spocs;
    // Undefined is fine here. It's optional and only used by collections.
    // If we leave it out, you get a collection that cannot be dismissed.
    const { flight_id } = spocs;
    return {
      items,
      title,
      context,
      sponsor,
      sponsored_by_override,
      ...(flight_id ? { flight_id } : {}),
    };
  }

  // This sets an override pref for personalization version.
  personalizationVersionOverride(spoc_v2) {
    const overrideVersion = this.store.getState().Prefs.values[
      PREF_PERSONALIZATION_OVERRIDE_VERSION
    ];

    const currentVersion = this.store.getState().Prefs.values[
      PREF_PERSONALIZATION_VERSION
    ];

    // If we have a downgrade override, and the current version can be downgraded,
    // and it hasn't already been downgraded, set it to 1.
    if (spoc_v2 === false && currentVersion === 2 && overrideVersion !== 1) {
      this.store.dispatch(ac.SetPref(PREF_PERSONALIZATION_OVERRIDE_VERSION, 1));
    }

    // This is if we need to revert the downgrade and do cleanup.
    if (spoc_v2 && overrideVersion === 1) {
      this.store.dispatch({
        type: at.CLEAR_PREF,
        data: { name: PREF_PERSONALIZATION_OVERRIDE_VERSION },
      });
    }
  }

  async loadSpocs(sendUpdate, isStartup) {
    const cachedData = (await this.cache.get()) || {};
    let spocsState;

    const { placements } = this.store.getState().DiscoveryStream.spocs;

    if (this.showSpocs) {
      spocsState = cachedData.spocs;
      if (this.isExpired({ cachedData, key: "spocs", isStartup })) {
        const endpoint = this.store.getState().DiscoveryStream.spocs
          .spocs_endpoint;

        const headers = new Headers();
        headers.append("content-type", "application/json");

        const apiKeyPref = this._prefCache.config.api_key_pref;
        const apiKey = Services.prefs.getCharPref(apiKeyPref, "");

        const spocsResponse = await this.fetchFromEndpoint(endpoint, {
          method: "POST",
          headers,
          body: JSON.stringify({
            pocket_id: this._impressionId,
            version: 2,
            consumer_key: apiKey,
            ...(placements.length ? { placements } : {}),
          }),
        });

        if (spocsResponse) {
          spocsState = {
            lastUpdated: Date.now(),
            spocs: {
              ...spocsResponse,
            },
          };

          if (spocsResponse.settings && spocsResponse.settings.feature_flags) {
            this.personalizationVersionOverride(
              spocsResponse.settings.feature_flags.spoc_v2
            );
          }

          const spocsResultPromises = this.getPlacements().map(
            async placement => {
              const freshSpocs = spocsState.spocs[placement.name];

              if (!freshSpocs) {
                return;
              }

              // spocs can be returns as an array, or an object with an items array.
              // We want to normalize this so all our spocs have an items array.
              // There can also be some meta data for title and context.
              // This is mostly because of backwards compat.
              const {
                items: normalizedSpocsItems,
                title,
                context,
                sponsor,
                sponsored_by_override,
              } = this.normalizeSpocsItems(freshSpocs);

              if (!normalizedSpocsItems || !normalizedSpocsItems.length) {
                // In the case of old data, we still want to ensure we normalize the data structure,
                // even if it's empty. We expect the empty data to be an object with items array,
                // and not just an empty array.
                spocsState.spocs = {
                  ...spocsState.spocs,
                  [placement.name]: {
                    title,
                    context,
                    items: [],
                  },
                };
                return;
              }

              // Migrate flight_id
              const { data: migratedSpocs } = this.migrateFlightId(
                normalizedSpocsItems
              );

              const { data: capResult } = this.frequencyCapSpocs(migratedSpocs);

              const { data: blockedResults } = this.filterBlocked(capResult);

              const { data: scoredResults } = await this.scoreItems(
                blockedResults,
                "spocs"
              );

              spocsState.spocs = {
                ...spocsState.spocs,
                [placement.name]: {
                  title,
                  context,
                  sponsor,
                  sponsored_by_override,
                  items: scoredResults,
                },
              };
            }
          );
          await Promise.all(spocsResultPromises);

          this.cleanUpFlightImpressionPref(spocsState.spocs);
          await this.cache.set("spocs", {
            lastUpdated: spocsState.lastUpdated,
            spocs: spocsState.spocs,
          });
        } else {
          Cu.reportError("No response for spocs_endpoint prop");
        }
      }
    }

    // Use good data if we have it, otherwise nothing.
    // We can have no data if spocs set to off.
    // We can have no data if request fails and there is no good cache.
    // We want to send an update spocs or not, so client can render something.
    spocsState =
      spocsState && spocsState.spocs
        ? spocsState
        : {
            lastUpdated: Date.now(),
            spocs: {},
          };

    sendUpdate({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: spocsState.lastUpdated,
        spocs: spocsState.spocs,
      },
      meta: {
        isStartup,
      },
    });
  }

  async clearSpocs() {
    const endpoint = this.store.getState().Prefs.values[
      PREF_SPOCS_CLEAR_ENDPOINT
    ];
    if (!endpoint) {
      return;
    }
    const headers = new Headers();
    headers.append("content-type", "application/json");

    await this.fetchFromEndpoint(endpoint, {
      method: "DELETE",
      headers,
      body: JSON.stringify({
        pocket_id: this._impressionId,
      }),
    });
  }

  /*
   * This just re hydrates the provider from cache.
   * We can call this on startup because it's generally fast.
   * It reports to devtools the last time the data in the cache was updated.
   */
  async loadAffinityScoresCache(isStartup = false) {
    const cachedData = (await this.cache.get()) || {};
    const { affinities } = cachedData;
    if (this.personalized && affinities && affinities.scores) {
      this.providerSwitcher.setAffinityProvider(
        affinities.timeSegments,
        affinities.parameterSets,
        affinities.maxHistoryQueryResults,
        affinities.version,
        affinities.scores
      );

      this.domainAffinitiesLastUpdated = affinities._timestamp;

      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
          data: {
            lastUpdated: this.domainAffinitiesLastUpdated,
          },
          meta: {
            isStartup,
          },
        })
      );
    }
  }

  /*
   * This creates a new affinityProvider using fresh affinities,
   * It's run on a last updated timer. This is the opposite of loadAffinityScoresCache.
   * This is also much slower so we only trigger this in the background on idle-daily.
   * It causes new profiles to pick up personalization slowly because the first time
   * a new profile is run you don't have any old cache to use, so it needs to wait for the first
   * idle-daily. Older profiles can rely on cache during the idle-daily gap. Idle-daily is
   * usually run once every 24 hours.
   */
  async updateDomainAffinityScores() {
    if (
      !this.personalized ||
      !this.affinities ||
      !this.affinities.parameterSets ||
      Date.now() - this.domainAffinitiesLastUpdated <
        MIN_DOMAIN_AFFINITIES_UPDATE_TIME
    ) {
      return;
    }

    this.providerSwitcher.setAffinityProvider(
      this.affinities.timeSegments,
      this.affinities.parameterSets,
      this.affinities.maxHistoryQueryResults,
      this.affinities.version,
      undefined
    );

    await this.providerSwitcher.init();

    const affinities = this.providerSwitcher.getAffinities();
    this.domainAffinitiesLastUpdated = Date.now();

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
        data: {
          lastUpdated: this.domainAffinitiesLastUpdated,
        },
      })
    );
    affinities._timestamp = this.domainAffinitiesLastUpdated;
    this.cache.set("affinities", affinities);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "idle-daily":
        this.updateDomainAffinityScores();
        break;
    }
  }

  /*
   * This function is used to sort any type of story, both spocs and recs.
   * This uses hierarchical sorting, first sorting by priority, then by score within a priority.
   * This function could be sorting an array of spocs or an array of recs.
   * A rec would have priority undefined, and a spoc would probably have a priority set.
   * Priority is sorted ascending, so low numbers are the highest priority.
   * Score is sorted descending, so high numbers are the highest score.
   * Undefined priority values are considered the lowest priority.
   * A negative priority is considered the same as undefined, lowest priority.
   * A negative priority is unlikely and not currently supported or expected.
   * A negative score is a possible use case.
   */
  sortItem(a, b) {
    // If the priorities are the same, sort based on score.
    // If both item priorities are undefined,
    // we can safely sort via score.
    if (a.priority === b.priority) {
      return b.score - a.score;
    } else if (!a.priority || a.priority <= 0) {
      // If priority is undefined or an unexpected value,
      // consider it lowest priority.
      return 1;
    } else if (!b.priority || b.priority <= 0) {
      // Also consider this case lowest priority.
      return -1;
    }
    // Our primary sort for items with priority.
    return a.priority - b.priority;
  }

  async scoreItems(items, type) {
    const spocsPersonalized = this.store.getState().Prefs.values[
      PREF_SPOCS_PERSONALIZED
    ];
    const recsPersonalized = this.store.getState().Prefs.values[
      PREF_RECS_PERSONALIZED
    ];
    const personalizedByType =
      type === "feed" ? recsPersonalized : spocsPersonalized;

    const data = (
      await Promise.all(
        items.map(item => this.scoreItem(item, personalizedByType))
      )
    )
      // Sort by highest scores.
      .sort(this.sortItem);

    return { data };
  }

  async scoreItem(item, personalizedByType) {
    item.score = item.item_score;
    if (item.score !== 0 && !item.score) {
      item.score = 1;
    }
    if (this.personalized && personalizedByType) {
      await this.providerSwitcher.calculateItemRelevanceScore(item);
    }
    return item;
  }

  filterBlocked(data) {
    if (data && data.length) {
      let flights = this.readDataPref(PREF_FLIGHT_BLOCKS);
      const filteredItems = data.filter(item => {
        const blocked =
          NewTabUtils.blockedLinks.isBlocked({ url: item.url }) ||
          flights[item.flight_id];
        return !blocked;
      });
      return { data: filteredItems };
    }
    return { data };
  }

  // For backwards compatibility, older spoc endpoint don't have flight_id,
  // but instead had campaign_id we can use
  //
  // @param {Object} data  An object that might have a SPOCS array.
  // @returns {Object} An object with a property `data` as the result.
  migrateFlightId(spocs) {
    if (spocs && spocs.length) {
      return {
        data: spocs.map(s => {
          return {
            ...s,
            ...(s.flight_id || s.campaign_id
              ? {
                  flight_id: s.flight_id || s.campaign_id,
                }
              : {}),
            ...(s.caps
              ? {
                  caps: {
                    ...s.caps,
                    flight: s.caps.flight || s.caps.campaign,
                  },
                }
              : {}),
          };
        }),
      };
    }
    return { data: spocs };
  }

  // Filter spocs based on frequency caps
  //
  // @param {Object} data  An object that might have a SPOCS array.
  // @returns {Object} An object with a property `data` as the result, and a property
  //                   `filterItems` as the frequency capped items.
  frequencyCapSpocs(spocs) {
    if (spocs && spocs.length) {
      const impressions = this.readDataPref(PREF_SPOC_IMPRESSIONS);
      const caps = [];
      const result = spocs.filter(s => {
        const isBelow = this.isBelowFrequencyCap(impressions, s);
        if (!isBelow) {
          caps.push(s);
        }
        return isBelow;
      });
      // send caps to redux if any.
      if (caps.length) {
        this.store.dispatch({
          type: at.DISCOVERY_STREAM_SPOCS_CAPS,
          data: caps,
        });
      }
      return { data: result, filtered: caps };
    }
    return { data: spocs, filtered: [] };
  }

  // Frequency caps are based on flight, which may include multiple spocs.
  // We currently support two types of frequency caps:
  // - lifetime: Indicates how many times spocs from a flight can be shown in total
  // - period: Indicates how many times spocs from a flight can be shown within a period
  //
  // So, for example, the feed configuration below defines that for flight 1 no more
  // than 5 spocs can be shown in total, and no more than 2 per hour.
  // "flight_id": 1,
  // "caps": {
  //  "lifetime": 5,
  //  "flight": {
  //    "count": 2,
  //    "period": 3600
  //  }
  // }
  isBelowFrequencyCap(impressions, spoc) {
    const flightImpressions = impressions[spoc.flight_id];
    if (!flightImpressions) {
      return true;
    }

    const lifetime = spoc.caps && spoc.caps.lifetime;

    const lifeTimeCap = Math.min(
      lifetime || MAX_LIFETIME_CAP,
      MAX_LIFETIME_CAP
    );
    const lifeTimeCapExceeded = flightImpressions.length >= lifeTimeCap;
    if (lifeTimeCapExceeded) {
      return false;
    }

    const flightCap = spoc.caps && spoc.caps.flight;
    if (flightCap) {
      const flightCapExceeded =
        flightImpressions.filter(i => Date.now() - i < flightCap.period * 1000)
          .length >= flightCap.count;
      return !flightCapExceeded;
    }
    return true;
  }

  async retryFeed(feed) {
    const { url } = feed;
    const result = await this.getComponentFeed(url);
    const newFeed = this.filterRecommendations(result);
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_FEED_UPDATE,
        data: {
          feed: newFeed,
          url,
        },
      })
    );
  }

  async getComponentFeed(feedUrl, isStartup) {
    const cachedData = (await this.cache.get()) || {};
    const { feeds } = cachedData;

    let feed = feeds ? feeds[feedUrl] : null;
    if (this.isExpired({ cachedData, key: "feed", url: feedUrl, isStartup })) {
      const feedResponse = await this.fetchFromEndpoint(feedUrl);
      if (feedResponse) {
        const { data: scoredItems } = await this.scoreItems(
          feedResponse.recommendations,
          "feed"
        );
        const { recsExpireTime } = feedResponse.settings;
        const recommendations = this.rotate(scoredItems, recsExpireTime);
        this.componentFeedFetched = true;
        feed = {
          lastUpdated: Date.now(),
          data: {
            settings: feedResponse.settings,
            recommendations,
            status: "success",
          },
        };
      } else {
        Cu.reportError("No response for feed");
      }
    }

    // If we have no feed at this point, both fetch and cache failed for some reason.
    return (
      feed || {
        data: {
          status: "failed",
        },
      }
    );
  }

  /**
   * Called at startup to update cached data in the background.
   */
  async _maybeUpdateCachedData() {
    const expirationPerComponent = await this._checkExpirationPerComponent();
    // Pass in `store.dispatch` to send the updates only to main
    if (expirationPerComponent.layout) {
      await this.loadLayout(this.store.dispatch);
    }
    if (expirationPerComponent.spocs) {
      await this.loadSpocs(this.store.dispatch);
    }
    if (expirationPerComponent.feeds) {
      await this.loadComponentFeeds(this.store.dispatch);
    }
  }

  /**
   * @typedef {Object} RefreshAll
   * @property {boolean} updateOpenTabs - Sends updates to open tabs immediately if true,
   *                                      updates in background if false
   * @property {boolean} isStartup - When the function is called at browser startup
   *
   * Refreshes layout, component feeds, and spocs in order if caches have expired.
   * @param {RefreshAll} options
   */
  async refreshAll(options = {}) {
    const affinityCacheLoadPromise = this.loadAffinityScoresCache(
      options.isStartup
    );

    const spocsPersonalized = this.store.getState().Prefs.values[
      PREF_SPOCS_PERSONALIZED
    ];
    const recsPersonalized = this.store.getState().Prefs.values[
      PREF_RECS_PERSONALIZED
    ];

    let expirationPerComponent = {};
    if (this.personalized) {
      // We store this before we refresh content.
      // This way, we can know what and if something got updated,
      // so we can know to score the results.
      expirationPerComponent = await this._checkExpirationPerComponent();
    }
    await this.refreshContent(options);

    if (this.personalized) {
      // affinityCacheLoadPromise is probably done, because of the refreshContent await above,
      // but to be sure, we should check that it's done, without making the parent function wait.
      affinityCacheLoadPromise.then(() => {
        // If we don't have expired stories or feeds, we don't need to score after init.
        // If we do have expired stories, we want to score after init.
        // In both cases, we don't want these to block the parent function.
        // This is why we store the promise, and call then to do our scoring work.
        const initPromise = this.providerSwitcher.init();
        initPromise.then(() => {
          // Both scoreFeeds and scoreSpocs are promises,
          // but they don't need to wait for each other.
          // We can just fire them and forget at this point.
          const { feeds, spocs } = this.store.getState().DiscoveryStream;
          if (
            recsPersonalized &&
            feeds.loaded &&
            expirationPerComponent.feeds
          ) {
            this.scoreFeeds(feeds);
          }
          if (
            spocsPersonalized &&
            spocs.loaded &&
            expirationPerComponent.spocs
          ) {
            this.scoreSpocs(spocs);
          }
        });
      });
    }
  }

  async scoreFeeds(feedsState) {
    if (feedsState.data) {
      const feeds = {};
      const feedsPromises = Object.keys(feedsState.data).map(url => {
        let feed = feedsState.data[url];
        const feedPromise = this.scoreItems(feed.data.recommendations, "feed");
        feedPromise.then(({ data: scoredItems }) => {
          const { recsExpireTime } = feed.data.settings;
          const recommendations = this.rotate(scoredItems, recsExpireTime);
          feed = {
            ...feed,
            data: {
              ...feed.data,
              recommendations,
            },
          };

          feeds[url] = feed;

          this.store.dispatch(
            ac.AlsoToPreloaded({
              type: at.DISCOVERY_STREAM_FEED_UPDATE,
              data: {
                feed,
                url,
              },
            })
          );
        });
        return feedPromise;
      });
      await Promise.all(feedsPromises);
      await this.cache.set("feeds", feeds);
    }
  }

  async scoreSpocs(spocsState) {
    const spocsResultPromises = this.getPlacements().map(async placement => {
      const nextSpocs = spocsState.data[placement.name] || {};
      const { items } = nextSpocs;

      if (!items || !items.length) {
        return;
      }

      const { data: scoreResult } = await this.scoreItems(items, "spocs");

      spocsState.data = {
        ...spocsState.data,
        [placement.name]: {
          ...nextSpocs,
          items: scoreResult,
        },
      };
    });
    await Promise.all(spocsResultPromises);

    // Update cache here so we don't need to re calculate scores on loads from cache.
    // Related Bug 1606276
    await this.cache.set("spocs", {
      lastUpdated: spocsState.lastUpdated,
      spocs: spocsState.data,
    });
    this.store.dispatch(
      ac.AlsoToPreloaded({
        type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
        data: {
          lastUpdated: spocsState.lastUpdated,
          spocs: spocsState.data,
        },
      })
    );
  }

  async refreshContent(options = {}) {
    const { updateOpenTabs, isStartup } = options;

    const dispatch = updateOpenTabs
      ? action => this.store.dispatch(ac.BroadcastToContent(action))
      : this.store.dispatch;

    await this.loadLayout(dispatch, isStartup);
    if (this.showStories) {
      await Promise.all([
        this.loadSpocs(dispatch, isStartup).catch(error =>
          Cu.reportError(`Error trying to load spocs feeds: ${error}`)
        ),
        this.loadComponentFeeds(dispatch, isStartup).catch(error =>
          Cu.reportError(`Error trying to load component feeds: ${error}`)
        ),
      ]);
      if (isStartup) {
        await this._maybeUpdateCachedData();
      }
    }
  }

  // We have to rotate stories on the client so that
  // active stories are at the front of the list, followed by stories that have expired
  // impressions i.e. have been displayed for longer than recsExpireTime.
  rotate(recommendations, recsExpireTime) {
    const maxImpressionAge = Math.max(
      recsExpireTime * 1000 || DEFAULT_RECS_EXPIRE_TIME,
      DEFAULT_RECS_EXPIRE_TIME
    );
    const impressions = this.readDataPref(PREF_REC_IMPRESSIONS);
    const expired = [];
    const active = [];
    for (const item of recommendations) {
      if (
        impressions[item.id] &&
        Date.now() - impressions[item.id] >= maxImpressionAge
      ) {
        expired.push(item);
      } else {
        active.push(item);
      }
    }
    return active.concat(expired);
  }

  enableStories() {
    if (this.config.enabled && this.loaded) {
      // If stories are being re enabled, ensure we have stories.
      this.refreshAll({ updateOpenTabs: true });
    }
  }

  async enable() {
    await this.refreshAll({ updateOpenTabs: true, isStartup: true });
    Services.obs.addObserver(this, "idle-daily");
    this.loaded = true;
  }

  async reset() {
    this.resetDataPrefs();
    await this.resetCache();
    if (this.loaded) {
      Services.obs.removeObserver(this, "idle-daily");
    }
    this.resetState();
  }

  async resetCache() {
    await this.resetAllCache();
  }

  async resetContentCache() {
    await this.cache.set("layout", {});
    await this.cache.set("feeds", {});
    await this.cache.set("spocs", {});
  }

  async resetAllCache() {
    await this.resetContentCache();
    await this.cache.set("affinities", {});
  }

  resetDataPrefs() {
    this.writeDataPref(PREF_SPOC_IMPRESSIONS, {});
    this.writeDataPref(PREF_REC_IMPRESSIONS, {});
    this.writeDataPref(PREF_FLIGHT_BLOCKS, {});
  }

  resetState() {
    // Reset reducer
    this.store.dispatch(
      ac.BroadcastToContent({ type: at.DISCOVERY_STREAM_LAYOUT_RESET })
    );
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE,
        data: {
          value: this.store.getState().Prefs.values[
            PREF_COLLECTION_DISMISSIBLE
          ],
        },
      })
    );
    this.domainAffinitiesLastUpdated = null;
    this.loaded = false;
  }

  async onPrefChange() {
    // We always want to clear the cache/state if the pref has changed
    await this.reset();
    if (this.config.enabled) {
      // Load data from all endpoints
      await this.enable();
    }
  }

  // This is a request to change the config from somewhere.
  // Can be from a spefic pref related to Discovery Stream,
  // or can be a generic request from an external feed that
  // something changed.
  configReset() {
    this._prefCache.config = null;
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
        data: this.config,
      })
    );
  }

  recordFlightImpression(flightId) {
    let impressions = this.readDataPref(PREF_SPOC_IMPRESSIONS);

    const timeStamps = impressions[flightId] || [];
    timeStamps.push(Date.now());
    impressions = { ...impressions, [flightId]: timeStamps };

    this.writeDataPref(PREF_SPOC_IMPRESSIONS, impressions);
  }

  recordTopRecImpressions(recId) {
    let impressions = this.readDataPref(PREF_REC_IMPRESSIONS);
    if (!impressions[recId]) {
      impressions = { ...impressions, [recId]: Date.now() };
      this.writeDataPref(PREF_REC_IMPRESSIONS, impressions);
    }
  }

  recordBlockFlightId(flightId) {
    const flights = this.readDataPref(PREF_FLIGHT_BLOCKS);
    if (!flights[flightId]) {
      flights[flightId] = 1;
      this.writeDataPref(PREF_FLIGHT_BLOCKS, flights);
    }
  }

  cleanUpFlightImpressionPref(data) {
    let flightIds = [];
    this.placementsForEach(placement => {
      const newSpocs = data[placement.name];
      if (!newSpocs) {
        return;
      }

      const items = newSpocs.items || [];
      flightIds = [...flightIds, ...items.map(s => `${s.flight_id}`)];
    });
    if (flightIds && flightIds.length) {
      this.cleanUpImpressionPref(
        id => !flightIds.includes(id),
        PREF_SPOC_IMPRESSIONS
      );
    }
  }

  // Clean up rec impression pref by removing all stories that are no
  // longer part of the response.
  cleanUpTopRecImpressionPref(newFeeds) {
    // Need to build a single list of stories.
    const activeStories = Object.keys(newFeeds)
      .filter(currentValue => newFeeds[currentValue].data)
      .reduce((accumulator, currentValue) => {
        const { recommendations } = newFeeds[currentValue].data;
        return accumulator.concat(recommendations.map(i => `${i.id}`));
      }, []);
    this.cleanUpImpressionPref(
      id => !activeStories.includes(id),
      PREF_REC_IMPRESSIONS
    );
  }

  writeDataPref(pref, impressions) {
    this.store.dispatch(ac.SetPref(pref, JSON.stringify(impressions)));
  }

  readDataPref(pref) {
    const prefVal = this.store.getState().Prefs.values[pref];
    return prefVal ? JSON.parse(prefVal) : {};
  }

  cleanUpImpressionPref(isExpired, pref) {
    const impressions = this.readDataPref(pref);
    let changed = false;

    Object.keys(impressions).forEach(id => {
      if (isExpired(id)) {
        changed = true;
        delete impressions[id];
      }
    });

    if (changed) {
      this.writeDataPref(pref, impressions);
    }
  }

  async onPrefChangedAction(action) {
    switch (action.data.name) {
      case PREF_CONFIG:
      case PREF_ENABLED:
      case PREF_HARDCODED_BASIC_LAYOUT:
      case PREF_SPOCS_ENDPOINT:
      case PREF_SPOCS_ENDPOINT_QUERY:
        // This is a config reset directly related to Discovery Stream pref.
        this.configReset();
        break;
      case PREF_USER_TOPSTORIES:
      case PREF_SYSTEM_TOPSTORIES:
        if (!action.data.value) {
          // Ensure we delete any remote data potentially related to spocs.
          this.clearSpocs();
        } else {
          this.enableStories();
        }
        break;
      // Check if spocs was disabled. Remove them if they were.
      case PREF_SHOW_SPONSORED:
        if (!action.data.value) {
          // Ensure we delete any remote data potentially related to spocs.
          this.clearSpocs();
        }
        await this.loadSpocs(update =>
          this.store.dispatch(ac.BroadcastToContent(update))
        );
        break;
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        // During the initialization of Firefox:
        // 1. Set-up listeners and initialize the redux state for config;
        this.setupPrefs(true /* isStartup */);
        // 2. If config.enabled is true, start loading data.
        if (this.config.enabled) {
          await this.enable();
        }
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        // Only refresh if we loaded once in .enable()
        if (
          this.config.enabled &&
          this.loaded &&
          (await this.checkIfAnyCacheExpired())
        ) {
          await this.refreshAll({ updateOpenTabs: false });
        }
        break;
      case at.DISCOVERY_STREAM_DEV_IDLE_DAILY:
        Services.obs.notifyObservers(null, "idle-daily");
        break;
      case at.DISCOVERY_STREAM_DEV_SYNC_RS:
        RemoteSettings.pollChanges();
        break;
      case at.DISCOVERY_STREAM_DEV_EXPIRE_CACHE:
        // Affinities update at a slower interval than content, so in order to debug,
        // we want to be able to expire just content to trigger the earlier expire times.
        await this.resetContentCache();
        break;
      case at.DISCOVERY_STREAM_CONFIG_SET_VALUE:
        // Use the original string pref to then set a value instead of
        // this.config which has some modifications
        this.store.dispatch(
          ac.SetPref(
            PREF_CONFIG,
            JSON.stringify({
              ...JSON.parse(this.store.getState().Prefs.values[PREF_CONFIG]),
              [action.data.name]: action.data.value,
            })
          )
        );
        break;

      case at.DISCOVERY_STREAM_CONFIG_RESET:
        // This is a generic config reset likely related to an external feed pref.
        this.configReset();
        break;
      case at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS:
        this.resetConfigDefauts();
        break;
      case at.DISCOVERY_STREAM_RETRY_FEED:
        this.retryFeed(action.data.feed);
        break;
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        // When the config pref changes, load or unload data as needed.
        await this.onPrefChange();
        break;
      case at.DISCOVERY_STREAM_IMPRESSION_STATS:
        if (
          action.data.tiles &&
          action.data.tiles[0] &&
          action.data.tiles[0].id
        ) {
          this.recordTopRecImpressions(action.data.tiles[0].id);
        }
        break;
      case at.DISCOVERY_STREAM_SPOC_IMPRESSION:
        if (this.showSpocs) {
          this.recordFlightImpression(action.data.flightId);

          // Apply frequency capping to SPOCs in the redux store, only update the
          // store if the SPOCs are changed.
          const spocsState = this.store.getState().DiscoveryStream.spocs;

          let frequencyCapped = [];
          this.placementsForEach(placement => {
            const spocs = spocsState.data[placement.name];
            if (!spocs || !spocs.items) {
              return;
            }

            const { data: capResult, filtered } = this.frequencyCapSpocs(
              spocs.items
            );
            frequencyCapped = [...frequencyCapped, ...filtered];

            spocsState.data = {
              ...spocsState.data,
              [placement.name]: {
                ...spocs,
                items: capResult,
              },
            };
          });

          if (frequencyCapped.length) {
            // Update cache here so we don't need to re calculate frequency caps on loads from cache.
            await this.cache.set("spocs", {
              lastUpdated: spocsState.lastUpdated,
              spocs: spocsState.data,
            });

            this.store.dispatch(
              ac.AlsoToPreloaded({
                type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
                data: {
                  lastUpdated: spocsState.lastUpdated,
                  spocs: spocsState.data,
                },
              })
            );
          }
        }
        break;
      // This is fired from the browser, it has no concept of spocs, flight or pocket.
      // We match the blocked url with our available spoc urls to see if there is a match.
      // I suspect we *could* instead do this in BLOCK_URL but I'm not sure.
      case at.PLACES_LINK_BLOCKED:
        if (this.showSpocs) {
          let blockedItems = [];
          const spocsState = this.store.getState().DiscoveryStream.spocs;

          this.placementsForEach(placement => {
            const spocs = spocsState.data[placement.name];
            if (spocs && spocs.items && spocs.items.length) {
              const blockedResults = [];
              const blocks = spocs.items.filter(s => {
                const blocked = s.url === action.data.url;
                if (!blocked) {
                  blockedResults.push(s);
                }
                return blocked;
              });

              blockedItems = [...blockedItems, ...blocks];

              spocsState.data = {
                ...spocsState.data,
                [placement.name]: {
                  ...spocs,
                  items: blockedResults,
                },
              };
            }
          });

          if (blockedItems.length) {
            // Update cache here so we don't need to re calculate blocks on loads from cache.
            await this.cache.set("spocs", {
              lastUpdated: spocsState.lastUpdated,
              spocs: spocsState.data,
            });

            // If we're blocking a spoc, we want open tabs to have
            // a slightly different treatment from future tabs.
            // AlsoToPreloaded updates the source data and preloaded tabs with a new spoc.
            // BroadcastToContent updates open tabs with a non spoc instead of a new spoc.
            this.store.dispatch(
              ac.AlsoToPreloaded({
                type: at.DISCOVERY_STREAM_LINK_BLOCKED,
                data: action.data,
              })
            );
            this.store.dispatch(
              ac.BroadcastToContent({
                type: at.DISCOVERY_STREAM_SPOC_BLOCKED,
                data: action.data,
              })
            );
            break;
          }
        }

        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.DISCOVERY_STREAM_LINK_BLOCKED,
            data: action.data,
          })
        );
        break;
      case at.UNINIT:
        // When this feed is shutting down:
        this.uninitPrefs();
        this._providerSwitcher = null;
        break;
      case at.BLOCK_URL: {
        // If we block a story that also has a flight_id
        // we want to record that as blocked too.
        // This is because a single flight might have slightly different urls.
        action.data.forEach(site => {
          const { flight_id } = site;
          if (flight_id) {
            this.recordBlockFlightId(flight_id);
          }
        });
        break;
      }
      case at.PREF_CHANGED:
        await this.onPrefChangedAction(action);
        break;
    }
  }
};

// This function generates a hardcoded layout each call.
// This is because modifying the original object would
// persist across pref changes and system_tick updates.
//
// NOTE: There is some branching logic in the template based on `isBasicLayout`
//
getHardcodedLayout = isBasicLayout => ({
  lastUpdate: Date.now(),
  spocs: {
    url: "https://spocs.getpocket.com/spocs",
  },
  layout: [
    {
      width: 12,
      components: [
        {
          type: "TopSites",
          header: {
            title: {
              id: "newtab-section-header-topsites",
            },
          },
          properties: {},
        },
        {
          type: "CollectionCardGrid",
          properties: {
            items: 3,
          },
          header: {
            title: "",
          },
          placement: {
            name: "sponsored-collection",
            ad_types: [3617],
            zone_ids: [217759, 218031],
          },
          spocs: {
            probability: 1,
            positions: [
              {
                index: 0,
              },
              {
                index: 1,
              },
              {
                index: 2,
              },
            ],
          },
        },
        {
          type: "Message",
          header: {
            title: {
              id: "newtab-section-header-pocket",
              values: { provider: "Pocket" },
            },
            subtitle: "",
            link_text: {
              id: "newtab-pocket-learn-more",
            },
            link_url: "https://getpocket.com/firefox/new_tab_learn_more",
            icon:
              "chrome://activity-stream/content/data/content/assets/glyph-pocket-16.svg",
          },
          properties: {},
          styles: {
            ".ds-message": "margin-bottom: -20px",
          },
        },
        {
          type: "CardGrid",
          properties: {
            items: isBasicLayout ? 3 : 21,
          },
          cta_variant: "link",
          header: {
            title: "",
          },
          placement: {
            name: "spocs",
            ad_types: [3617],
            zone_ids: [217758, 217995],
          },
          feed: {
            embed_reference: null,
            url:
              "https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=3&consumer_key=$apiKey&locale_lang=$locale&region=$region&count=30",
          },
          spocs: {
            probability: 1,
            positions: [
              {
                index: 2,
              },
              {
                index: 4,
              },
              {
                index: 11,
              },
              {
                index: 20,
              },
            ],
          },
        },
        {
          type: "Navigation",
          properties: {
            alignment: "left-align",
            links: [
              {
                name: "Self Improvement",
                url:
                  "https://getpocket.com/explore/self-improvement?utm_source=pocket-newtab",
              },
              {
                name: "Food",
                url:
                  "https://getpocket.com/explore/food?utm_source=pocket-newtab",
              },
              {
                name: "Entertainment",
                url:
                  "https://getpocket.com/explore/entertainment?utm_source=pocket-newtab",
              },
              {
                name: "Health",
                url:
                  "https://getpocket.com/explore/health?utm_source=pocket-newtab",
              },
              {
                name: "Science",
                url:
                  "https://getpocket.com/explore/science?utm_source=pocket-newtab",
              },
              {
                name: "More Recommendations ›",
                url: "https://getpocket.com/explore?utm_source=pocket-newtab",
              },
            ],
            privacyNoticeURL: {
              url:
                "https://www.mozilla.org/privacy/firefox/#suggest-relevant-content",
              title: {
                id: "newtab-section-menu-privacy-notice",
              },
            },
          },
          header: {
            title: {
              id: "newtab-pocket-read-more",
            },
          },
          styles: {
            ".ds-navigation": "margin-top: -10px;",
          },
        },
      ],
    },
  ],
});

const EXPORTED_SYMBOLS = ["DiscoveryStreamFeed"];
