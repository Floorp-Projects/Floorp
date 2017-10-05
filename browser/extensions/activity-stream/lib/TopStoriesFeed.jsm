/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {shortURL} = Cu.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {SectionsManager} = Cu.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {UserDomainAffinityProvider} = Cu.import("resource://activity-stream/lib/UserDomainAffinityProvider.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "perfService", "resource://activity-stream/common/PerfService.jsm");

const STORIES_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const TOPICS_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3 hours
const DOMAIN_AFFINITY_UPDATE_TIME = 24 * 60 * 60 * 1000; // 24 hours
const STORIES_NOW_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours
const SECTION_ID = "topstories";

this.TopStoriesFeed = class TopStoriesFeed {
  constructor() {
    this.spocsPerNewTabs = 0;
    this.newTabsSinceSpoc = 0;
    this.contentUpdateQueue = [];
  }

  init() {
    const initFeed = () => {
      SectionsManager.enableSection(SECTION_ID);
      try {
        const options = SectionsManager.sections.get(SECTION_ID).options;
        const apiKey = this.getApiKeyFromPref(options.api_key_pref);
        this.stories_endpoint = this.produceFinalEndpointUrl(options.stories_endpoint, apiKey);
        this.topics_endpoint = this.produceFinalEndpointUrl(options.topics_endpoint, apiKey);
        this.read_more_endpoint = options.read_more_endpoint;
        this.stories_referrer = options.stories_referrer;
        this.personalized = options.personalized;
        this.show_spocs = options.show_spocs;
        this.maxHistoryQueryResults = options.maxHistoryQueryResults;
        this.storiesLastUpdated = 0;
        this.topicsLastUpdated = 0;
        this.affinityLastUpdated = 0;

        this.fetchStories();
        this.fetchTopics();
      } catch (e) {
        Cu.reportError(`Problem initializing top stories feed: ${e.message}`);
      }
    };
    SectionsManager.onceInitialized(initFeed);
  }

  uninit() {
    SectionsManager.disableSection(SECTION_ID);
  }

  async fetchStories() {
    if (!this.stories_endpoint) {
      return;
    }
    try {
      const response = await fetch(this.stories_endpoint);
      if (!response.ok) {
        throw new Error(`Stories endpoint returned unexpected status: ${response.status}`);
      }

      const body = await response.json();
      this.updateSettings(body.settings);
      this.stories = this.rotate(this.transform(body.recommendations));
      this.spocs = this.show_spocs && this.transform(body.spocs).filter(s => s.score >= s.min_score);

      this.dispatchUpdateEvent(this.storiesLastUpdated, {rows: this.stories});
      this.storiesLastUpdated = Date.now();
      // This is filtered so an update function can return true to retry on the next run
      this.contentUpdateQueue = this.contentUpdateQueue.filter(update => update());
    } catch (error) {
      Cu.reportError(`Failed to fetch content: ${error.message}`);
    }
  }

  transform(items) {
    if (!items) {
      return [];
    }

    return items
      .filter(s => !NewTabUtils.blockedLinks.isBlocked({"url": s.url}))
      .map(s => ({
        "guid": s.id,
        "hostname": shortURL(Object.assign({}, s, {url: s.url})),
        "type": (Date.now() - (s.published_timestamp * 1000)) <= STORIES_NOW_THRESHOLD ? "now" : "trending",
        "context": s.context,
        "icon": s.icon,
        "title": s.title,
        "description": s.excerpt,
        "image": this.normalizeUrl(s.image_src),
        "referrer": this.stories_referrer,
        "url": s.url,
        "min_score": s.min_score || 0,
        "score": this.personalized ? this.affinityProvider.calculateItemRelevanceScore(s) : 1
      }))
      .sort(this.personalized ? this.compareScore : (a, b) => 0);
  }

  async fetchTopics() {
    if (!this.topics_endpoint) {
      return;
    }
    try {
      const response = await fetch(this.topics_endpoint);
      if (!response.ok) {
        throw new Error(`Topics endpoint returned unexpected status: ${response.status}`);
      }
      const {topics} = await response.json();
      if (topics) {
        this.dispatchUpdateEvent(this.topicsLastUpdated, {topics, read_more_endpoint: this.read_more_endpoint});
        this.topicsLastUpdated = Date.now();
      }
    } catch (error) {
      Cu.reportError(`Failed to fetch topics: ${error.message}`);
    }
  }

  dispatchUpdateEvent(lastUpdated, data) {
    SectionsManager.updateSection(SECTION_ID, data, lastUpdated === 0);
  }

  compareScore(a, b) {
    return b.score - a.score;
  }

  updateSettings(settings) {
    if (!this.personalized) {
      return;
    }

    this.spocsPerNewTabs = settings.spocsPerNewTabs;

    if (!this.affinityProvider || (Date.now() - this.affinityLastUpdated >= DOMAIN_AFFINITY_UPDATE_TIME)) {
      const start = perfService.absNow();
      this.affinityProvider = new UserDomainAffinityProvider(
        settings.timeSegments,
        settings.domainAffinityParameterSets,
        this.maxHistoryQueryResults);

      this.store.dispatch(ac.PerfEvent({
        event: "topstories.domain.affinity.calculation.ms",
        value: Math.round(perfService.absNow() - start)
      }));
      this.affinityLastUpdated = Date.now();
    }
  }

  // If personalization is turned on we have to rotate stories on the client.
  // An item can only be on top for two iterations (1hr) before it gets moved
  // to the end. This will later be improved based on interactions/impressions.
  rotate(items) {
    if (!this.personalized || items.length <= 3) {
      return items;
    }

    if (!this.topItems) {
      this.topItems = new Map();
    }

    // This avoids an infinite recursion if for some reason the feed stops
    // changing. Otherwise, there's a chance we'd be rotating forever to
    // find an item we haven't displayed on top yet.
    if (this.topItems.size >= items.length) {
      this.topItems.clear();
    }

    const guid = items[0].guid;
    if (!this.topItems.has(guid)) {
      this.topItems.set(guid, 0);
    } else {
      const val = this.topItems.get(guid) + 1;
      this.topItems.set(guid, val);
      if (val >= 2) {
        items.push(items.shift());
        this.rotate(items);
      }
    }
    return items;
  }

  getApiKeyFromPref(apiKeyPref) {
    if (!apiKeyPref) {
      return apiKeyPref;
    }

    return new Prefs().get(apiKeyPref) || Services.prefs.getCharPref(apiKeyPref);
  }

  produceFinalEndpointUrl(url, apiKey) {
    if (!url) {
      return url;
    }
    if (url.includes("$apiKey") && !apiKey) {
      throw new Error(`An API key was specified but none configured: ${url}`);
    }
    return url.replace("$apiKey", apiKey);
  }

  // Need to remove parenthesis from image URLs as React will otherwise
  // fail to render them properly as part of the card template.
  normalizeUrl(url) {
    if (url) {
      return url.replace(/\(/g, "%28").replace(/\)/g, "%29");
    }
    return url;
  }

  maybeAddSpoc(target) {
    if (!this.show_spocs) {
      return;
    }

    if (this.newTabsSinceSpoc === 0 || this.newTabsSinceSpoc === this.spocsPerNewTabs) {
      const updateContent = () => {
        if (!this.spocs || !this.spocs.length) {
          // We have stories but no spocs so there's nothing to do and this update can be
          // removed from the queue.
          return false;
        }

        // Create a new array with a spoc inserted at index 2
        // For now we're using the top scored spoc until we can support viewability based rotation
        const position = SectionsManager.sections.get(SECTION_ID).order;
        let rows = this.store.getState().Sections[position].rows.slice(0, this.stories.length);
        rows.splice(2, 0, this.spocs[0]);

        // Send a content update to the target tab
        const action = {type: at.SECTION_UPDATE, meta: {skipMain: true}, data: Object.assign({rows}, {id: SECTION_ID})};
        this.store.dispatch(ac.SendToContent(action, target));
        return false;
      };

      if (this.stories) {
        updateContent();
      } else {
        // Delay updating tab content until initial data has been fetched
        this.contentUpdateQueue.push(updateContent);
      }

      this.newTabsSinceSpoc = 0;
    }
    this.newTabsSinceSpoc++;
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.SYSTEM_TICK:
        if (Date.now() - this.storiesLastUpdated >= STORIES_UPDATE_TIME) {
          this.fetchStories();
        }
        if (Date.now() - this.topicsLastUpdated >= TOPICS_UPDATE_TIME) {
          this.fetchTopics();
        }
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.NEW_TAB_REHYDRATED:
        this.maybeAddSpoc(action.meta.fromTarget);
        break;
      case at.SECTION_OPTIONS_CHANGED:
        if (action.data === SECTION_ID) {
          this.uninit();
          this.init();
        }
        break;
      case at.PLACES_LINK_BLOCKED:
        if (this.spocs) {
          this.spocs = this.spocs.filter(s => s.url !== action.data.url);
        }
        break;
    }
  }
};

this.STORIES_UPDATE_TIME = STORIES_UPDATE_TIME;
this.TOPICS_UPDATE_TIME = TOPICS_UPDATE_TIME;
this.SECTION_ID = SECTION_ID;
this.EXPORTED_SYMBOLS = ["TopStoriesFeed", "STORIES_UPDATE_TIME", "TOPICS_UPDATE_TIME", "DOMAIN_AFFINITY_UPDATE_TIME", "SECTION_ID"];
