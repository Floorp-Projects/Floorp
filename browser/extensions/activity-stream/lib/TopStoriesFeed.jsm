/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});

const STORIES_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const TOPICS_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3 hours
const STORIES_NOW_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours
const SECTION_ID = "TopStories";

this.TopStoriesFeed = class TopStoriesFeed {
  constructor() {
    this.storiesLastUpdated = 0;
    this.topicsLastUpdated = 0;
  }

  init() {
    try {
      const prefs = new Prefs();
      const options = JSON.parse(prefs.get("feeds.section.topstories.options"));
      const apiKey = this._getApiKeyFromPref(options.api_key_pref);
      this.stories_endpoint = this._produceUrlWithApiKey(options.stories_endpoint, apiKey);
      this.topics_endpoint = this._produceUrlWithApiKey(options.topics_endpoint, apiKey);
      this.read_more_endpoint = options.read_more_endpoint;
      this.stories_referrer = options.stories_referrer;

      // TODO https://github.com/mozilla/activity-stream/issues/2902
      const sectionOptions = {
        id: SECTION_ID,
        eventSource: "TOP_STORIES",
        icon: options.provider_icon,
        title: {id: "header_recommended_by", values: {provider: options.provider_name}},
        rows: [],
        maxCards: 3,
        contextMenuOptions: ["CheckBookmark", "SaveToPocket", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"],
        infoOption: {
          header: {id: "pocket_feedback_header"},
          body: {id: options.provider_description},
          link: {
            href: options.survey_link,
            id: "pocket_send_feedback"
          }
        },
        emptyState: {
          message: {id: "topstories_empty_state", values: {provider: options.provider_name}},
          icon: "check"
        }
      };
      this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_REGISTER, data: sectionOptions}));

      this.fetchStories();
      this.fetchTopics();
    } catch (e) {
      Cu.reportError(`Problem initializing top stories feed: ${e.message}`);
    }
  }

  uninit() {
    this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_DEREGISTER, data: SECTION_ID}));
  }

  async fetchStories() {
    if (this.stories_endpoint) {
      const stories = await fetch(this.stories_endpoint)
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error(`Stories endpoint returned unexpected status: ${response.status}`);
        })
        .then(body => {
          let items = JSON.parse(body).list;
          items = items
            .filter(s => !NewTabUtils.blockedLinks.isBlocked({"url": s.dedupe_url}))
            .map(s => ({
              "guid": s.id,
              "type": (Date.now() - (s.published_timestamp * 1000)) <= STORIES_NOW_THRESHOLD ? "now" : "trending",
              "title": s.title,
              "description": s.excerpt,
              "image": this._normalizeUrl(s.image_src),
              "referrer": this.stories_referrer,
              "url": s.dedupe_url
            }));
          return items;
        })
        .catch(error => Cu.reportError(`Failed to fetch content: ${error.message}`));

      if (stories) {
        this.dispatchUpdateEvent(this.storiesLastUpdated,
          {"type": at.SECTION_ROWS_UPDATE, "data": {"id": SECTION_ID, "rows": stories}});
        this.storiesLastUpdated = Date.now();
      }
    }
  }

  async fetchTopics() {
    if (this.topics_endpoint) {
      const topics = await fetch(this.topics_endpoint)
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error(`Topics endpoint returned unexpected status: ${response.status}`);
        })
        .then(body => JSON.parse(body).topics)
        .catch(error => Cu.reportError(`Failed to fetch topics: ${error.message}`));

      if (topics) {
        this.dispatchUpdateEvent(this.topicsLastUpdated,
          {"type": at.SECTION_ROWS_UPDATE, "data": {"id": SECTION_ID, "topics": topics, "read_more_endpoint": this.read_more_endpoint}});
        this.topicsLastUpdated = Date.now();
      }
    }
  }

  dispatchUpdateEvent(lastUpdated, evt) {
    if (lastUpdated === 0) {
      this.store.dispatch(ac.BroadcastToContent(evt));
    } else {
      this.store.dispatch(evt);
    }
  }

  _getApiKeyFromPref(apiKeyPref) {
    if (!apiKeyPref) {
      return apiKeyPref;
    }

    return new Prefs().get(apiKeyPref) || Services.prefs.getCharPref(apiKeyPref);
  }

  _produceUrlWithApiKey(url, apiKey) {
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
      case at.FEED_INIT:
        if (action.data === "feeds.section.topstories") {
          this.init();
        }
        break;
    }
  }
};

this.STORIES_UPDATE_TIME = STORIES_UPDATE_TIME;
this.TOPICS_UPDATE_TIME = TOPICS_UPDATE_TIME;
this.SECTION_ID = SECTION_ID;
this.EXPORTED_SYMBOLS = ["TopStoriesFeed", "STORIES_UPDATE_TIME", "TOPICS_UPDATE_TIME", "SECTION_ID"];
