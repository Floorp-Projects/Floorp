/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NewTabUtils } = ChromeUtils.import(
  "resource://gre/modules/NewTabUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { Prefs } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm"
);
const { shortURL } = ChromeUtils.import(
  "resource://activity-stream/lib/ShortURL.jsm"
);
const { SectionsManager } = ChromeUtils.import(
  "resource://activity-stream/lib/SectionsManager.jsm"
);
const { UserDomainAffinityProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/UserDomainAffinityProvider.jsm"
);
const { PersonalityProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PersonalityProvider/PersonalityProvider.jsm"
);
const { PersistentCache } = ChromeUtils.import(
  "resource://activity-stream/lib/PersistentCache.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "pktApi",
  "chrome://pocket/content/pktApi.jsm"
);

const STORIES_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const TOPICS_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3 hours
const STORIES_NOW_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours
const MIN_DOMAIN_AFFINITIES_UPDATE_TIME = 12 * 60 * 60 * 1000; // 12 hours
const DEFAULT_RECS_EXPIRE_TIME = 60 * 60 * 1000; // 1 hour
const SECTION_ID = "topstories";
const IMPRESSION_SOURCE = "TOP_STORIES";
const SPOC_IMPRESSION_TRACKING_PREF =
  "feeds.section.topstories.spoc.impressions";
const DISCOVERY_STREAM_PREF_ENABLED = "discoverystream.enabled";
const DISCOVERY_STREAM_PREF_ENABLED_PATH =
  "browser.newtabpage.activity-stream.discoverystream.enabled";
const REC_IMPRESSION_TRACKING_PREF = "feeds.section.topstories.rec.impressions";
const OPTIONS_PREF = "feeds.section.topstories.options";
const MAX_LIFETIME_CAP = 500; // Guard against misconfiguration on the server
const DISCOVERY_STREAM_PREF = "discoverystream.config";

this.TopStoriesFeed = class TopStoriesFeed {
  constructor(ds) {
    // Use discoverystream config pref default values for fast path and
    // if needed lazy load activity stream top stories feed based on
    // actual user preference when INIT and PREF_CHANGED is invoked
    this.discoveryStreamEnabled =
      ds &&
      ds.value &&
      JSON.parse(ds.value).enabled &&
      Services.prefs.getBoolPref(DISCOVERY_STREAM_PREF_ENABLED_PATH, false);
    if (!this.discoveryStreamEnabled) {
      this.initializeProperties();
    }
  }

  initializeProperties() {
    this.contentUpdateQueue = [];
    this.spocCampaignMap = new Map();
    this.cache = new PersistentCache(SECTION_ID, true);
    this._prefs = new Prefs();
    this.propertiesInitialized = true;
  }

  async onInit() {
    SectionsManager.enableSection(SECTION_ID);
    if (this.discoveryStreamEnabled) {
      return;
    }

    try {
      const { options } = SectionsManager.sections.get(SECTION_ID);
      const apiKey = this.getApiKeyFromPref(options.api_key_pref);
      this.stories_endpoint = this.produceFinalEndpointUrl(
        options.stories_endpoint,
        apiKey
      );
      this.topics_endpoint = this.produceFinalEndpointUrl(
        options.topics_endpoint,
        apiKey
      );
      this.read_more_endpoint = options.read_more_endpoint;
      this.stories_referrer = options.stories_referrer;
      this.personalized = options.personalized;
      this.show_spocs = options.show_spocs;
      this.maxHistoryQueryResults = options.maxHistoryQueryResults;
      this.storiesLastUpdated = 0;
      this.topicsLastUpdated = 0;
      this.storiesLoaded = false;
      this.domainAffinitiesLastUpdated = 0;
      this.processAffinityProividerVersion(options);
      this.dispatchPocketCta(this._prefs.get("pocketCta"), false);
      Services.obs.addObserver(this, "idle-daily");

      // Cache is used for new page loads, which shouldn't have changed data.
      // If we have changed data, cache should be cleared,
      // and last updated should be 0, and we can fetch.
      let { stories, topics } = await this.loadCachedData();
      if (this.storiesLastUpdated === 0) {
        stories = await this.fetchStories();
      }
      if (this.topicsLastUpdated === 0) {
        topics = await this.fetchTopics();
      }
      this.doContentUpdate({ stories, topics }, true);
      this.storiesLoaded = true;

      // This is filtered so an update function can return true to retry on the next run
      this.contentUpdateQueue = this.contentUpdateQueue.filter(update =>
        update()
      );
    } catch (e) {
      Cu.reportError(`Problem initializing top stories feed: ${e.message}`);
    }
  }

  init() {
    SectionsManager.onceInitialized(this.onInit.bind(this));
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "idle-daily":
        this.updateDomainAffinityScores();
        break;
    }
  }

  async clearCache() {
    await this.cache.set("stories", {});
    await this.cache.set("topics", {});
    await this.cache.set("spocs", {});
  }

  uninit() {
    this.storiesLoaded = false;
    try {
      Services.obs.removeObserver(this, "idle-daily");
    } catch (e) {
      // Attempt to remove unassociated observer which is possible when discovery stream
      // is enabled and user never used activity stream experience
    }
    SectionsManager.disableSection(SECTION_ID);
  }

  getPocketState(target) {
    const action = { type: at.POCKET_LOGGED_IN, data: pktApi.isUserLoggedIn() };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  dispatchPocketCta(data, shouldBroadcast) {
    const action = { type: at.POCKET_CTA, data: JSON.parse(data) };
    this.store.dispatch(
      shouldBroadcast
        ? ac.BroadcastToContent(action)
        : ac.AlsoToPreloaded(action)
    );
  }

  /**
   * doContentUpdate - Updates topics and stories in the topstories section.
   *
   *                   Sections have one update action for the whole section.
   *                   Redux creates a state race condition if you call the same action,
   *                   twice, concurrently. Because of this, doContentUpdate is
   *                   one place to update both topics and stories in a single action.
   *
   *                   Section updates used old topics if none are available,
   *                   but clear stories if none are available. Because of this, if no
   *                   stories are passed, we instead use the existing stories in state.
   *
   * @param {Object} This is an object with potential new stories or topics.
   * @param {Boolean} shouldBroadcast If we should update existing tabs or not. For first page
   *                  loads or pref changes, we want to update existing tabs,
   *                  for system tick or other updates we do not.
   */
  doContentUpdate({ stories, topics }, shouldBroadcast) {
    let updateProps = {};
    if (stories) {
      updateProps.rows = stories;
    } else {
      const { Sections } = this.store.getState();
      if (Sections && Sections.find) {
        updateProps.rows = Sections.find(s => s.id === SECTION_ID).rows;
      }
    }
    if (topics) {
      Object.assign(updateProps, {
        topics,
        read_more_endpoint: this.read_more_endpoint,
      });
    }

    // We should only be calling this once per init.
    this.dispatchUpdateEvent(shouldBroadcast, updateProps);
  }

  async onPersonalityProviderInit() {
    const data = await this.cache.get();
    let stories = data.stories && data.stories.recommendations;
    this.stories = this.rotate(this.transform(stories));
    this.doContentUpdate({ stories: this.stories }, false);

    const affinities = this.affinityProvider.getAffinities();
    this.domainAffinitiesLastUpdated = Date.now();
    affinities._timestamp = this.domainAffinitiesLastUpdated;
    this.cache.set("domainAffinities", affinities);
  }

  affinityProividerSwitcher(...args) {
    const { affinityProviderV2 } = this;
    if (affinityProviderV2 && affinityProviderV2.use_v2) {
      const provider = this.PersonalityProvider(...args, {
        modelKeys: affinityProviderV2.model_keys,
        dispatch: this.store.dispatch,
      });
      provider.init(this.onPersonalityProviderInit.bind(this));
      return provider;
    }

    const start = Cu.now();
    const v1Provider = this.UserDomainAffinityProvider(...args);
    this.store.dispatch(
      ac.PerfEvent({
        event: "topstories.domain.affinity.calculation.ms",
        value: Math.round(Cu.now() - start),
      })
    );

    return v1Provider;
  }

  PersonalityProvider(...args) {
    return new PersonalityProvider(...args);
  }

  UserDomainAffinityProvider(...args) {
    return new UserDomainAffinityProvider(...args);
  }

  async fetchStories() {
    if (!this.stories_endpoint) {
      return null;
    }
    try {
      const response = await fetch(this.stories_endpoint, {
        credentials: "omit",
      });
      if (!response.ok) {
        throw new Error(
          `Stories endpoint returned unexpected status: ${response.status}`
        );
      }

      const body = await response.json();
      this.updateSettings(body.settings);
      this.stories = this.rotate(this.transform(body.recommendations));
      this.cleanUpTopRecImpressionPref();

      if (this.show_spocs && body.spocs) {
        this.spocCampaignMap = new Map(
          body.spocs.map(s => [s.id, `${s.campaign_id}`])
        );
        this.spocs = this.transform(body.spocs).filter(
          s => s.score >= s.min_score
        );
        this.cleanUpCampaignImpressionPref();
      }
      this.storiesLastUpdated = Date.now();
      body._timestamp = this.storiesLastUpdated;
      this.cache.set("stories", body);
    } catch (error) {
      Cu.reportError(`Failed to fetch content: ${error.message}`);
    }
    return this.stories;
  }

  async loadCachedData() {
    const data = await this.cache.get();
    let stories = data.stories && data.stories.recommendations;
    let topics = data.topics && data.topics.topics;

    let affinities = data.domainAffinities;
    if (this.personalized && affinities && affinities.scores) {
      this.affinityProvider = this.affinityProividerSwitcher(
        affinities.timeSegments,
        affinities.parameterSets,
        affinities.maxHistoryQueryResults,
        affinities.version,
        affinities.scores
      );
      this.domainAffinitiesLastUpdated = affinities._timestamp;
    }
    if (stories && !!stories.length && this.storiesLastUpdated === 0) {
      this.updateSettings(data.stories.settings);
      this.stories = this.rotate(this.transform(stories));
      this.storiesLastUpdated = data.stories._timestamp;
      if (data.stories.spocs && data.stories.spocs.length) {
        this.spocCampaignMap = new Map(
          data.stories.spocs.map(s => [s.id, `${s.campaign_id}`])
        );
        this.spocs = this.transform(data.stories.spocs).filter(
          s => s.score >= s.min_score
        );
        this.cleanUpCampaignImpressionPref();
      }
    }
    if (topics && !!topics.length && this.topicsLastUpdated === 0) {
      this.topics = topics;
      this.topicsLastUpdated = data.topics._timestamp;
    }

    return { topics: this.topics, stories: this.stories };
  }

  dispatchRelevanceScore(start) {
    let event = "PERSONALIZATION_V1_ITEM_RELEVANCE_SCORE_DURATION";
    let initialized = true;
    if (!this.personalized) {
      return;
    }
    const { affinityProviderV2 } = this;
    if (affinityProviderV2 && affinityProviderV2.use_v2) {
      if (this.affinityProvider) {
        initialized = this.affinityProvider.initialized;
        event = "PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION";
      }
    }

    // If v2 is not yet initialized we don't bother tracking yet.
    // Before it is initialized it doesn't do any ranking.
    // Once it's initialized it ensures ranking is done.
    // v1 doesn't have any initialized issues around ranking,
    // and should be ready right away.
    if (initialized) {
      this.store.dispatch(
        ac.PerfEvent({
          event,
          value: Math.round(Cu.now() - start),
        })
      );
    }
  }

  transform(items) {
    if (!items) {
      return [];
    }

    const scoreStart = Cu.now();
    const calcResult = items
      .filter(s => !NewTabUtils.blockedLinks.isBlocked({ url: s.url }))
      .map(s => {
        let mapped = {
          guid: s.id,
          hostname: s.domain || shortURL(Object.assign({}, s, { url: s.url })),
          type:
            Date.now() - s.published_timestamp * 1000 <= STORIES_NOW_THRESHOLD
              ? "now"
              : "trending",
          context: s.context,
          icon: s.icon,
          title: s.title,
          description: s.excerpt,
          image: this.normalizeUrl(s.image_src),
          referrer: this.stories_referrer,
          url: s.url,
          min_score: s.min_score || 0,
          score:
            this.personalized && this.affinityProvider
              ? this.affinityProvider.calculateItemRelevanceScore(s)
              : s.item_score || 1,
          spoc_meta: this.show_spocs
            ? { campaign_id: s.campaign_id, caps: s.caps }
            : {},
        };

        // Very old cached spocs may not contain an `expiration_timestamp` property
        if (s.expiration_timestamp) {
          mapped.expiration_timestamp = s.expiration_timestamp;
        }

        return mapped;
      })
      .sort(this.personalized ? this.compareScore : (a, b) => 0);

    this.dispatchRelevanceScore(scoreStart);
    return calcResult;
  }

  async fetchTopics() {
    if (!this.topics_endpoint) {
      return null;
    }
    try {
      const response = await fetch(this.topics_endpoint, {
        credentials: "omit",
      });
      if (!response.ok) {
        throw new Error(
          `Topics endpoint returned unexpected status: ${response.status}`
        );
      }
      const body = await response.json();
      const { topics } = body;
      if (topics) {
        this.topics = topics;
        this.topicsLastUpdated = Date.now();
        body._timestamp = this.topicsLastUpdated;
        this.cache.set("topics", body);
      }
    } catch (error) {
      Cu.reportError(`Failed to fetch topics: ${error.message}`);
    }
    return this.topics;
  }

  dispatchUpdateEvent(shouldBroadcast, data) {
    SectionsManager.updateSection(SECTION_ID, data, shouldBroadcast);
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

    if (
      this.affinityProvider &&
      this.affinityProvider.version !== this.version
    ) {
      this.resetDomainAffinityScores();
    }
  }

  updateDomainAffinityScores() {
    if (
      !this.personalized ||
      !this.domainAffinityParameterSets ||
      Date.now() - this.domainAffinitiesLastUpdated <
        MIN_DOMAIN_AFFINITIES_UPDATE_TIME
    ) {
      return;
    }

    this.affinityProvider = this.affinityProividerSwitcher(
      this.timeSegments,
      this.domainAffinityParameterSets,
      this.maxHistoryQueryResults,
      this.version,
      undefined
    );

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

    const maxImpressionAge = Math.max(
      this.recsExpireTime * 1000 || DEFAULT_RECS_EXPIRE_TIME,
      DEFAULT_RECS_EXPIRE_TIME
    );
    const impressions = this.readImpressionsPref(REC_IMPRESSION_TRACKING_PREF);
    const expired = [];
    const active = [];
    for (const item of items) {
      if (
        impressions[item.guid] &&
        Date.now() - impressions[item.guid] >= maxImpressionAge
      ) {
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

    return (
      this._prefs.get(apiKeyPref) || Services.prefs.getCharPref(apiKeyPref)
    );
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

  dispatchSpocDone(target) {
    const action = { type: at.POCKET_WAITING_FOR_SPOC, data: false };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  filterSpocs() {
    if (!this.shouldShowSpocs()) {
      return [];
    }

    if (Math.random() > this.spocsPerNewTabs) {
      return [];
    }

    if (!this.spocs || !this.spocs.length) {
      // We have stories but no spocs so there's nothing to do and this update can be
      // removed from the queue.
      return [];
    }

    // Filter spocs based on frequency caps
    const impressions = this.readImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF);
    let spocs = this.spocs.filter(s =>
      this.isBelowFrequencyCap(impressions, s)
    );

    // Filter out expired spocs based on `expiration_timestamp`
    spocs = spocs.filter(spoc => {
      // If cached data is so old it doesn't contain this property, assume the spoc is ok to show
      if (!(`expiration_timestamp` in spoc)) {
        return true;
      }
      // `expiration_timestamp` is the number of seconds elapsed since January 1, 1970 00:00:00 UTC
      return spoc.expiration_timestamp * 1000 > Date.now();
    });

    return spocs;
  }

  maybeAddSpoc(target) {
    const updateContent = () => {
      let spocs = this.filterSpocs();

      if (!spocs.length) {
        this.dispatchSpocDone(target);
        return false;
      }

      // Create a new array with a spoc inserted at index 2
      const section = this.store
        .getState()
        .Sections.find(s => s.id === SECTION_ID);
      let rows = section.rows.slice(0, this.stories.length);
      rows.splice(2, 0, Object.assign(spocs[0], { pinned: true }));

      // Send a content update to the target tab
      const action = {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows }, { id: SECTION_ID }),
      };
      this.store.dispatch(ac.OnlyToOneContent(action, target));
      this.dispatchSpocDone(target);
      return false;
    };

    if (this.storiesLoaded) {
      updateContent();
    } else {
      // Delay updating tab content until initial data has been fetched
      this.contentUpdateQueue.push(updateContent);
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

    const lifeTimeCap = Math.min(
      spoc.spoc_meta.caps && spoc.spoc_meta.caps.lifetime,
      MAX_LIFETIME_CAP
    );
    const lifeTimeCapExceeded = campaignImpressions.length >= lifeTimeCap;
    if (lifeTimeCapExceeded) {
      return false;
    }

    const campaignCap =
      (spoc.spoc_meta.caps && spoc.spoc_meta.caps.campaign) || {};
    const campaignCapExceeded =
      campaignImpressions.filter(
        i => Date.now() - i < campaignCap.period * 1000
      ).length >= campaignCap.count;
    return !campaignCapExceeded;
  }

  // Clean up campaign impression pref by removing all campaigns that are no
  // longer part of the response, and are therefore considered inactive.
  cleanUpCampaignImpressionPref() {
    const campaignIds = new Set(this.spocCampaignMap.values());
    this.cleanUpImpressionPref(
      id => !campaignIds.has(id),
      SPOC_IMPRESSION_TRACKING_PREF
    );
  }

  // Clean up rec impression pref by removing all stories that are no
  // longer part of the response.
  cleanUpTopRecImpressionPref() {
    const activeStories = new Set(this.stories.map(s => `${s.guid}`));
    this.cleanUpImpressionPref(
      id => !activeStories.has(id),
      REC_IMPRESSION_TRACKING_PREF
    );
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

    Object.keys(impressions).forEach(id => {
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
    impressions = Object.assign(impressions, { [campaignId]: timeStamps });

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
        impressions = Object.assign(impressions, { [t]: Date.now() });
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

  async removeSpocs() {
    // Quick hack so that SPOCS are removed from all open and preloaded tabs when
    // they are disabled. The longer term fix should probably be to remove them
    // in the Reducer.
    await this.clearCache();
    this.uninit();
    this.init();
  }

  /**
   * Decides if we need to change the personality provider version or not.
   * Changes the version if it determines we need to.
   *
   * @param data {object} The top stories pref, we need version and model_keys
   * @return {boolean} Returns true only if the version was changed.
   */
  processAffinityProividerVersion(data) {
    const version2 = data.version === 2 && !this.affinityProviderV2;
    const version1 = data.version === 1 && this.affinityProviderV2;
    if (version2 || version1) {
      if (version1) {
        this.affinityProviderV2 = null;
      } else {
        this.affinityProviderV2 = {
          use_v2: true,
          model_keys: data.model_keys,
        };
      }
      return true;
    }
    return false;
  }

  lazyLoadTopStories(dsPref) {
    let _dsPref = dsPref;
    if (!_dsPref) {
      _dsPref = this.store.getState().Prefs.values[DISCOVERY_STREAM_PREF];
    }

    try {
      this.discoveryStreamEnabled =
        JSON.parse(_dsPref).enabled &&
        this.store.getState().Prefs.values[DISCOVERY_STREAM_PREF_ENABLED];
    } catch (e) {
      // Load activity stream top stories if fail to determine discovery stream state
      this.discoveryStreamEnabled = false;
    }

    // Return without invoking initialization if top stories are loaded
    if (this.storiesLoaded) {
      return;
    }

    if (!this.discoveryStreamEnabled && !this.propertiesInitialized) {
      this.initializeProperties();
    }
    this.init();
  }

  handleDisabled(action) {
    switch (action.type) {
      case at.INIT:
        this.lazyLoadTopStories();
        break;
      case at.PREF_CHANGED:
        if (action.data.name === DISCOVERY_STREAM_PREF) {
          this.lazyLoadTopStories(action.data.value);
        }
        if (action.data.name === DISCOVERY_STREAM_PREF_ENABLED) {
          this.lazyLoadTopStories();
        }
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }

  async onAction(action) {
    if (this.discoveryStreamEnabled) {
      this.handleDisabled(action);
      return;
    }
    switch (action.type) {
      // Check discoverystream pref and load activity stream top stories only if needed
      case at.INIT:
        this.lazyLoadTopStories();
        break;
      case at.SYSTEM_TICK:
        let stories;
        let topics;
        if (Date.now() - this.storiesLastUpdated >= STORIES_UPDATE_TIME) {
          stories = await this.fetchStories();
        }
        if (Date.now() - this.topicsLastUpdated >= TOPICS_UPDATE_TIME) {
          topics = await this.fetchTopics();
        }
        this.doContentUpdate({ stories, topics }, false);
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.NEW_TAB_REHYDRATED:
        this.getPocketState(action.meta.fromTarget);
        this.maybeAddSpoc(action.meta.fromTarget);
        break;
      case at.SECTION_OPTIONS_CHANGED:
        if (action.data === SECTION_ID) {
          await this.clearCache();
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
        // We want to make sure we only track impressions from Top Stories,
        // otherwise unexpected things that are not properly handled can happen.
        // Example: Impressions from spocs on Discovery Stream can cause the
        // Top Stories impressions pref to continuously grow, see bug #1523408
        if (action.data.source === IMPRESSION_SOURCE) {
          const payload = action.data;
          const viewImpression = !(
            "click" in payload ||
            "block" in payload ||
            "pocket" in payload
          );
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
        }
        break;
      }
      case at.PREF_CHANGED:
        if (action.data.name === DISCOVERY_STREAM_PREF) {
          this.lazyLoadTopStories(action.data.value);
        }
        // Check if spocs was disabled. Remove them if they were.
        if (action.data.name === "showSponsored" && !action.data.value) {
          await this.removeSpocs();
        }
        if (action.data.name === "pocketCta") {
          this.dispatchPocketCta(action.data.value, true);
        }
        if (action.data.name === OPTIONS_PREF) {
          try {
            const options = JSON.parse(action.data.value);
            if (this.processAffinityProividerVersion(options)) {
              await this.clearCache();
              this.uninit();
              this.init();
            }
          } catch (e) {
            Cu.reportError(
              `Problem initializing affinity provider v2: ${e.message}`
            );
          }
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
const EXPORTED_SYMBOLS = [
  "TopStoriesFeed",
  "STORIES_UPDATE_TIME",
  "TOPICS_UPDATE_TIME",
  "SECTION_ID",
  "SPOC_IMPRESSION_TRACKING_PREF",
  "MIN_DOMAIN_AFFINITIES_UPDATE_TIME",
  "REC_IMPRESSION_TRACKING_PREF",
  "DEFAULT_RECS_EXPIRE_TIME",
];
