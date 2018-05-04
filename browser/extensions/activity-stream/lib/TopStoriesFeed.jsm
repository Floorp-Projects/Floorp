/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NewTabUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

const {actionTypes: at, actionCreators: ac} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = ChromeUtils.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {shortURL} = ChromeUtils.import("resource://activity-stream/lib/ShortURL.jsm", {});
const {SectionsManager} = ChromeUtils.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {UserDomainAffinityProvider} = ChromeUtils.import("resource://activity-stream/lib/UserDomainAffinityProvider.jsm", {});
const {PersistentCache} = ChromeUtils.import("resource://activity-stream/lib/PersistentCache.jsm", {});

ChromeUtils.defineModuleGetter(this, "perfService", "resource://activity-stream/common/PerfService.jsm");

const STORIES_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const TOPICS_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3 hours
const STORIES_NOW_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours
const MIN_DOMAIN_AFFINITIES_UPDATE_TIME = 12 * 60 * 60 * 1000; // 12 hours
const DEFAULT_RECS_EXPIRE_TIME = 60 * 60 * 1000; // 1 hour
const SECTION_ID = "topstories";
const SPOC_IMPRESSION_TRACKING_PREF = "feeds.section.topstories.spoc.impressions";
const REC_IMPRESSION_TRACKING_PREF = "feeds.section.topstories.rec.impressions";
const MAX_LIFETIME_CAP = 100; // Guard against misconfiguration on the server

this.TopStoriesFeed = class TopStoriesFeed {
  constructor() {
    this.spocCampaignMap = new Map();
    this.contentUpdateQueue = [];
    this.cache = new PersistentCache(SECTION_ID, true);
    this._prefs = new Prefs();
  }

  init() {
    const initFeed = () => {
      SectionsManager.enableSection(SECTION_ID);
      try {
        const {options} = SectionsManager.sections.get(SECTION_ID);
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
        this.domainAffinitiesLastUpdated = 0;

        this.loadCachedData();
        this.fetchStories();
        this.fetchTopics();

        Services.obs.addObserver(this, "idle-daily");
      } catch (e) {
        Cu.reportError(`Problem initializing top stories feed: ${e.message}`);
      }
    };
    SectionsManager.onceInitialized(initFeed);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "idle-daily":
        this.updateDomainAffinityScores();
        break;
    }
  }

  uninit() {
    Services.obs.removeObserver(this, "idle-daily");
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
      this.cleanUpTopRecImpressionPref();

      if (this.show_spocs && body.spocs) {
        this.spocCampaignMap = new Map(body.spocs.map(s => [s.id, `${s.campaign_id}`]));
        this.spocs = this.transform(body.spocs).filter(s => s.score >= s.min_score);
        this.cleanUpCampaignImpressionPref();
      }

      this.dispatchUpdateEvent(this.storiesLastUpdated, {rows: this.stories});
      this.storiesLastUpdated = Date.now();
      body._timestamp = this.storiesLastUpdated;
      // This is filtered so an update function can return true to retry on the next run
      this.contentUpdateQueue = this.contentUpdateQueue.filter(update => update());

      this.cache.set("stories", body);
    } catch (error) {
      Cu.reportError(`Failed to fetch content: ${error.message}`);
    }
  }

  async loadCachedData() {
    const data = await this.cache.get();
    let stories = data.stories && data.stories.recommendations;
    let topics = data.topics && data.topics.topics;
    let affinities = data.domainAffinities;
    if (this.personalized && affinities && affinities.scores) {
      this.affinityProvider = new UserDomainAffinityProvider(affinities.timeSegments,
        affinities.parameterSets, affinities.maxHistoryQueryResults, affinities.version, affinities.scores);
      this.domainAffinitiesLastUpdated = affinities._timestamp;
    }
    if (stories && stories.length > 0 && this.storiesLastUpdated === 0) {
      this.updateSettings(data.stories.settings);
      const rows = this.transform(stories);
      this.dispatchUpdateEvent(this.storiesLastUpdated, {rows});
      this.storiesLastUpdated = data.stories._timestamp;
    }
    if (topics && topics.length > 0 && this.topicsLastUpdated === 0) {
      this.dispatchUpdateEvent(this.topicsLastUpdated, {topics, read_more_endpoint: this.read_more_endpoint});
      this.topicsLastUpdated = data.topics._timestamp;
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
        "hostname": s.domain || shortURL(Object.assign({}, s, {url: s.url})),
        "type": (Date.now() - (s.published_timestamp * 1000)) <= STORIES_NOW_THRESHOLD ? "now" : "trending",
        "context": s.context,
        "icon": s.icon,
        "title": s.title,
        "description": s.excerpt,
        "image": this.normalizeUrl(s.image_src),
        "referrer": this.stories_referrer,
        "url": s.url,
        "min_score": s.min_score || 0,
        "score": this.personalized && this.affinityProvider ? this.affinityProvider.calculateItemRelevanceScore(s) : s.item_score || 1,
        "spoc_meta": this.show_spocs ? {campaign_id: s.campaign_id, caps: s.caps} : {}
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
      const body = await response.json();
      const {topics} = body;
      if (topics) {
        this.dispatchUpdateEvent(this.topicsLastUpdated, {topics, read_more_endpoint: this.read_more_endpoint});
        this.topicsLastUpdated = Date.now();
        body._timestamp = this.topicsLastUpdated;
        this.cache.set("topics", body);
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

    this.spocsPerNewTabs = settings.spocsPerNewTabs; // Probability of a new tab getting a spoc [0,1]
    this.timeSegments = settings.timeSegments;
    this.domainAffinityParameterSets = settings.domainAffinityParameterSets;
    this.recsExpireTime = settings.recsExpireTime;
    this.version = settings.version;

    if (this.affinityProvider && (this.affinityProvider.version !== this.version)) {
      this.resetDomainAffinityScores();
    }
  }

  updateDomainAffinityScores() {
    if (!this.personalized || !this.domainAffinityParameterSets ||
      Date.now() - this.domainAffinitiesLastUpdated < MIN_DOMAIN_AFFINITIES_UPDATE_TIME) {
      return;
    }

    const start = perfService.absNow();

    this.affinityProvider = new UserDomainAffinityProvider(
      this.timeSegments,
      this.domainAffinityParameterSets,
      this.maxHistoryQueryResults,
      this.version);

    this.store.dispatch(ac.PerfEvent({
      event: "topstories.domain.affinity.calculation.ms",
      value: Math.round(perfService.absNow() - start)
    }));

    const affinities = this.affinityProvider.getAffinities();
    this.domainAffinitiesLastUpdated = Date.now();
    affinities._timestamp = this.domainAffinitiesLastUpdated;
    this.cache.set("domainAffinities", affinities);
  }

  resetDomainAffinityScores() {
    delete this.affinityProvider;
    this.cache.set("domainAffinities", {});
  }

  // If personalization is turned on, we have to rotate stories on the client so that
  // active stories are at the front of the list, followed by stories that have expired
  // impressions i.e. have been displayed for longer than recsExpireTime.
  rotate(items) {
    if (!this.personalized || items.length <= 3) {
      return items;
    }

    const maxImpressionAge = Math.max(this.recsExpireTime * 1000 || DEFAULT_RECS_EXPIRE_TIME, DEFAULT_RECS_EXPIRE_TIME);
    const impressions = this.readImpressionsPref(REC_IMPRESSION_TRACKING_PREF);
    const expired = [];
    const active = [];
    for (const item of items) {
      if (impressions[item.guid] && Date.now() - impressions[item.guid] >= maxImpressionAge) {
        expired.push(item);
      } else {
        active.push(item);
      }
    }
    return active.concat(expired);
  }

  getApiKeyFromPref(apiKeyPref) {
    if (!apiKeyPref) {
      return apiKeyPref;
    }

    return this._prefs.get(apiKeyPref) || Services.prefs.getCharPref(apiKeyPref);
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

  shouldShowSpocs() {
    return this.show_spocs && this.store.getState().Prefs.values.showSponsored;
  }

  maybeAddSpoc(target) {
    if (!this.shouldShowSpocs()) {
      return;
    }

    if (Math.random() <= this.spocsPerNewTabs) {
      const updateContent = () => {
        if (!this.spocs || !this.spocs.length) {
          // We have stories but no spocs so there's nothing to do and this update can be
          // removed from the queue.
          return false;
        }

        // Filter spocs based on frequency caps
        const impressions = this.readImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF);
        const spocs = this.spocs.filter(s => this.isBelowFrequencyCap(impressions, s));

        if (!spocs.length) {
          // There's currently no spoc left to display
          return false;
        }

        // Create a new array with a spoc inserted at index 2
        const section = this.store.getState().Sections.find(s => s.id === SECTION_ID);
        let rows = section.rows.slice(0, this.stories.length);
        rows.splice(2, 0, Object.assign(spocs[0], {pinned: true}));

        // Send a content update to the target tab
        const action = {type: at.SECTION_UPDATE, data: Object.assign({rows}, {id: SECTION_ID})};
        this.store.dispatch(ac.OnlyToOneContent(action, target));
        return false;
      };

      if (this.stories) {
        updateContent();
      } else {
        // Delay updating tab content until initial data has been fetched
        this.contentUpdateQueue.push(updateContent);
      }
    }
  }

  // Frequency caps are based on campaigns, which may include multiple spocs.
  // We currently support two types of frequency caps:
  // - lifetime: Indicates how many times spocs from a campaign can be shown in total
  // - period: Indicates how many times spocs from a campaign can be shown within a period
  //
  // So, for example, the feed configuration below defines that for campaign 1 no more
  // than 5 spocs can be show in total, and no more than 2 per hour.
  // "campaign_id": 1,
  // "caps": {
  //  "lifetime": 5,
  //  "campaign": {
  //    "count": 2,
  //    "period": 3600
  //  }
  // }
  isBelowFrequencyCap(impressions, spoc) {
    const campaignImpressions = impressions[spoc.spoc_meta.campaign_id];
    if (!campaignImpressions) {
      return true;
    }

    const lifeTimeCap = Math.min(spoc.spoc_meta.caps && spoc.spoc_meta.caps.lifetime, MAX_LIFETIME_CAP);
    const lifeTimeCapExceeded = campaignImpressions.length >= lifeTimeCap;
    if (lifeTimeCapExceeded) {
      return false;
    }

    const campaignCap = (spoc.spoc_meta.caps && spoc.spoc_meta.caps.campaign) || {};
    const campaignCapExceeded = campaignImpressions
      .filter(i => (Date.now() - i) < (campaignCap.period * 1000)).length >= campaignCap.count;
    return !campaignCapExceeded;
  }

  // Clean up campaign impression pref by removing all campaigns that are no
  // longer part of the response, and are therefore considered inactive.
  cleanUpCampaignImpressionPref() {
    const campaignIds = new Set(this.spocCampaignMap.values());
    this.cleanUpImpressionPref(id => !campaignIds.has(id), SPOC_IMPRESSION_TRACKING_PREF);
  }

  // Clean up rec impression pref by removing all stories that are no
  // longer part of the response.
  cleanUpTopRecImpressionPref() {
    const activeStories = new Set(this.stories.map(s => `${s.guid}`));
    this.cleanUpImpressionPref(id => !activeStories.has(id), REC_IMPRESSION_TRACKING_PREF);
  }

  /**
   * Cleans up the provided impression pref (spocs or recs).
   *
   * @param isExpired predicate (boolean-valued function) that returns whether or not
   * the impression for the given key is expired.
   * @param pref the impression pref to clean up.
   */
  cleanUpImpressionPref(isExpired, pref) {
    const impressions = this.readImpressionsPref(pref);
    let changed = false;

    Object
      .keys(impressions)
      .forEach(id => {
        if (isExpired(id)) {
          changed = true;
          delete impressions[id];
        }
      });

    if (changed) {
      this.writeImpressionsPref(pref, impressions);
    }
  }

  // Sets a pref mapping campaign IDs to timestamp arrays.
  // The timestamps represent impressions which are used to calculate frequency caps.
  recordCampaignImpression(campaignId) {
    let impressions = this.readImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF);

    const timeStamps = impressions[campaignId] || [];
    timeStamps.push(Date.now());
    impressions = Object.assign(impressions, {[campaignId]: timeStamps});

    this.writeImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF, impressions);
  }

  // Sets a pref mapping story (rec) IDs to a single timestamp (time of first impression).
  // We use these timestamps to guarantee a story doesn't stay on top for longer than
  // configured in the feed settings (settings.recsExpireTime).
  recordTopRecImpressions(topItems) {
    let impressions = this.readImpressionsPref(REC_IMPRESSION_TRACKING_PREF);
    let changed = false;

    topItems.forEach(t => {
      if (!impressions[t]) {
        changed = true;
        impressions = Object.assign(impressions, {[t]: Date.now()});
      }
    });

    if (changed) {
      this.writeImpressionsPref(REC_IMPRESSION_TRACKING_PREF, impressions);
    }
  }

  readImpressionsPref(pref) {
    const prefVal = this._prefs.get(pref);
    return prefVal ? JSON.parse(prefVal) : {};
  }

  writeImpressionsPref(pref, impressions) {
    this._prefs.set(pref, JSON.stringify(impressions));
  }

  removeSpocs() {
    // Quick hack so that SPOCS are removed from all open and preloaded tabs when
    // they are disabled. The longer term fix should probably be to remove them
    // in the Reducer.
    this.uninit();
    this.init();
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
      case at.PLACES_HISTORY_CLEARED:
        if (this.personalized) {
          this.resetDomainAffinityScores();
        }
        break;
      case at.TELEMETRY_IMPRESSION_STATS: {
        const payload = action.data;
        const viewImpression = !("click" in payload || "block" in payload || "pocket" in payload);
        if (payload.tiles && viewImpression) {
          if (this.shouldShowSpocs()) {
            payload.tiles.forEach(t => {
              if (this.spocCampaignMap.has(t.id)) {
                this.recordCampaignImpression(this.spocCampaignMap.get(t.id));
              }
            });
          }
          if (this.personalized) {
            const topRecs = payload.tiles
              .filter(t => !this.spocCampaignMap.has(t.id))
              .map(t => t.id);
            this.recordTopRecImpressions(topRecs);
          }
        }
        break;
      }
      case at.PREF_CHANGED:
        // Check if spocs was disabled. Remove them if they were.
        if (action.data.name === "showSponsored" && !action.data.value) {
          this.removeSpocs();
        }
        break;
    }
  }
};

this.STORIES_UPDATE_TIME = STORIES_UPDATE_TIME;
this.TOPICS_UPDATE_TIME = TOPICS_UPDATE_TIME;
this.SECTION_ID = SECTION_ID;
this.SPOC_IMPRESSION_TRACKING_PREF = SPOC_IMPRESSION_TRACKING_PREF;
this.REC_IMPRESSION_TRACKING_PREF = REC_IMPRESSION_TRACKING_PREF;
this.MIN_DOMAIN_AFFINITIES_UPDATE_TIME = MIN_DOMAIN_AFFINITIES_UPDATE_TIME;
this.DEFAULT_RECS_EXPIRE_TIME = DEFAULT_RECS_EXPIRE_TIME;
const EXPORTED_SYMBOLS = ["TopStoriesFeed", "STORIES_UPDATE_TIME", "TOPICS_UPDATE_TIME", "SECTION_ID", "SPOC_IMPRESSION_TRACKING_PREF", "MIN_DOMAIN_AFFINITIES_UPDATE_TIME", "REC_IMPRESSION_TRACKING_PREF", "DEFAULT_RECS_EXPIRE_TIME"];
