/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {shortURL} = Cu.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {SectionsManager} = Cu.import("resource://activity-stream/lib/SectionsManager.jsm", {});

const {UserDomainAffinityProvider} = Cu.import("resource://activity-stream/lib/UserDomainAffinityProvider.jsm", {});

const STORIES_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const TOPICS_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3 hours
const DOMAIN_AFFINITY_UPDATE_TIME = 24 * 60 * 60 * 1000; // 24 hours
const STORIES_NOW_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours
const SECTION_ID = "topstories";

this.TopStoriesFeed = class TopStoriesFeed {

  init() {
    this.storiesLastUpdated = 0;
    this.topicsLastUpdated = 0;
    this.affinityLastUpdated = 0;

    SectionsManager.onceInitialized(this.parseOptions.bind(this));
  }

  parseOptions() {
    SectionsManager.enableSection(SECTION_ID);
    const options = SectionsManager.sections.get(SECTION_ID).options;
    try {
      const apiKey = this._getApiKeyFromPref(options.api_key_pref);
      this.stories_endpoint = this._produceFinalEndpointUrl(options.stories_endpoint, apiKey);
      this.topics_endpoint = this._produceFinalEndpointUrl(options.topics_endpoint, apiKey);
      this.read_more_endpoint = options.read_more_endpoint;
      this.stories_referrer = options.stories_referrer;
      this.personalized = options.personalized;
      this.maxHistoryQueryResults = options.maxHistoryQueryResults;

      this.fetchStories();
      this.fetchTopics();
    } catch (e) {
      Cu.reportError(`Problem initializing top stories feed: ${e.message}`);
    }
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
      this.updateDomainAffinities(body.settings);

      const recommendations = body.recommendations
        .filter(s => !NewTabUtils.blockedLinks.isBlocked({"url": s.url}))
        .map(s => ({
          "guid": s.id,
          "hostname": shortURL(Object.assign({}, s, {url: s.url})),
          "type": (Date.now() - (s.published_timestamp * 1000)) <= STORIES_NOW_THRESHOLD ? "now" : "trending",
          "title": s.title,
          "description": s.excerpt,
          "image": this._normalizeUrl(s.image_src),
          "referrer": this.stories_referrer,
          "url": s.url,
          "score": this.personalized ? this.affinityProvider.calculateItemRelevanceScore(s) : 1
        }))
        .sort(this.personalized ? this.compareScore : (a, b) => 0);

      const rows = this.rotate(recommendations);

      this.dispatchUpdateEvent(this.storiesLastUpdated, {rows});
      this.storiesLastUpdated = Date.now();
    } catch (error) {
      Cu.reportError(`Failed to fetch content: ${error.message}`);
    }
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

  updateDomainAffinities(settings) {
    if (!this.personalized) {
      return;
    }

    if (!this.affinityProvider || (Date.now() - this.affinityLastUpdated >= DOMAIN_AFFINITY_UPDATE_TIME)) {
      this.affinityProvider = new UserDomainAffinityProvider(
        settings.timeSegments,
        settings.domainAffinityParameterSets,
        this.maxHistoryQueryResults);
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

    const guid = items[0].guid;
    if (!this.topItem || !(guid in this.topItem)) {
      this.topItem = {[guid]: 0};
    } else if (++this.topItem[guid] === 2) {
      items.push(items.shift());
      this.topItem = {[items[0].guid]: 0};
    }
    return items;
  }

  _getApiKeyFromPref(apiKeyPref) {
    if (!apiKeyPref) {
      return apiKeyPref;
    }

    return new Prefs().get(apiKeyPref) || Services.prefs.getCharPref(apiKeyPref);
  }

  _produceFinalEndpointUrl(url, apiKey) {
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
  _normalizeUrl(url) {
    if (url) {
      return url.replace(/\(/g, "%28").replace(/\)/g, "%29");
    }
    return url;
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
    }
  }
};

this.STORIES_UPDATE_TIME = STORIES_UPDATE_TIME;
this.TOPICS_UPDATE_TIME = TOPICS_UPDATE_TIME;
this.SECTION_ID = SECTION_ID;
this.EXPORTED_SYMBOLS = ["TopStoriesFeed", "STORIES_UPDATE_TIME", "TOPICS_UPDATE_TIME", "DOMAIN_AFFINITY_UPDATE_TIME", "SECTION_ID"];
