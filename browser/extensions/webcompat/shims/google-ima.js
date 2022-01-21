/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 1713690 - Shim Google Interactive Media Ads ima3.js
 *
 * Many sites use ima3.js for ad bidding and placement, often in conjunction
 * with Google Publisher Tags, Prebid.js and/or other scripts. This shim
 * provides a stubbed-out version of the API which helps work around related
 * site breakage, such as black bxoes where videos ought to be placed.
 */

if (!window.google?.ima?.VERSION) {
  const VERSION = "3.453.0";

  let ima = {};

  class AdDisplayContainer {
    destroy() {}
    initialize() {}
  }

  class ImaSdkSettings {
    #c = true;
    #f = {};
    #i = false;
    #l = "";
    #p = "";
    #r = 0;
    #t = "";
    #v = "";
    getCompanionBackfill() {}
    getDisableCustomPlaybackForIOS10Plus() {
      return this.#i;
    }
    getFeatureFlags() {
      return this.#f;
    }
    getLocale() {
      return this.#l;
    }
    getNumRedirects() {
      return this.#r;
    }
    getPlayerType() {
      return this.#t;
    }
    getPlayerVersion() {
      return this.#v;
    }
    getPpid() {
      return this.#p;
    }
    isCookiesEnabled() {
      return this.#c;
    }
    setAutoPlayAdBreaks() {}
    setCompanionBackfill() {}
    setCookiesEnabled(c) {
      this.#c = !!c;
    }
    setDisableCustomPlaybackForIOS10Plus(i) {
      this.#i = !!i;
    }
    setFeatureFlags(f) {
      this.#f = f;
    }
    setLocale(l) {
      this.#l = l;
    }
    setNumRedirects(r) {
      this.#r = r;
    }
    setPlayerType(t) {
      this.#t = t;
    }
    setPlayerVersion(v) {
      this.#v = v;
    }
    setPpid(p) {
      this.#p = p;
    }
    setSessionId(s) {}
    setVpaidAllowed(a) {}
    setVpaidMode(m) {}
  }
  ImaSdkSettings.CompanionBackfillMode = {
    ALWAYS: "always",
    ON_MASTER_AD: "on_master_ad",
  };
  ImaSdkSettings.VpaidMode = {
    DISABLED: 0,
    ENABLED: 1,
    INSECURE: 2,
  };

  let managerLoaded = false;

  class EventHandler {
    #listeners = new Map();

    _dispatch(e) {
      const listeners = this.#listeners.get(e.type) || [];
      for (const listener of Array.from(listeners)) {
        try {
          listener(e);
        } catch (r) {
          console.error(r);
        }
      }
    }

    addEventListener(t, c) {
      if (!this.#listeners.has(t)) {
        this.#listeners.set(t, new Set());
      }
      this.#listeners.get(t).add(c);
    }

    removeEventListener(t, c) {
      this.#listeners.get(t)?.delete(c);
    }
  }

  class AdsLoader extends EventHandler {
    #settings = new ImaSdkSettings();
    contentComplete() {}
    destroy() {}
    getSettings() {
      return this.#settings;
    }
    getVersion() {
      return VERSION;
    }
    requestAds(r, c) {
      if (!managerLoaded) {
        managerLoaded = true;
        requestAnimationFrame(() => {
          const { ADS_MANAGER_LOADED } = AdsManagerLoadedEvent.Type;
          this._dispatch(new ima.AdsManagerLoadedEvent(ADS_MANAGER_LOADED));
        });
      }
    }
  }

  class AdsManager extends EventHandler {
    #volume = 1;
    collapse() {}
    configureAdsManager() {}
    destroy() {}
    discardAdBreak() {}
    expand() {}
    focus() {}
    getAdSkippableState() {
      return false;
    }
    getCuePoints() {
      return [0];
    }
    getCurrentAd() {
      return currentAd;
    }
    getCurrentAdCuePoints() {
      return [];
    }
    getRemainingTime() {
      return 0;
    }
    getVolume() {
      return this.#volume;
    }
    init(w, h, m, e) {}
    isCustomClickTrackingUsed() {
      return false;
    }
    isCustomPlaybackUsed() {
      return false;
    }
    pause() {}
    requestNextAdBreak() {}
    resize(w, h, m) {}
    resume() {}
    setVolume(v) {
      this.#volume = v;
    }
    skip() {}
    start() {
      requestAnimationFrame(() => {
        for (const type of [
          AdEvent.Type.LOADED,
          AdEvent.Type.STARTED,
          AdEvent.Type.AD_BUFFERING,
          AdEvent.Type.FIRST_QUARTILE,
          AdEvent.Type.MIDPOINT,
          AdEvent.Type.THIRD_QUARTILE,
          AdEvent.Type.COMPLETE,
          AdEvent.Type.ALL_ADS_COMPLETED,
        ]) {
          try {
            this._dispatch(new ima.AdEvent(type));
          } catch (e) {
            console.error(e);
          }
        }
      });
    }
    stop() {}
    updateAdsRenderingSettings(s) {}
  }

  class AdsRenderingSettings {}

  class AdsRequest {
    setAdWillAutoPlay() {}
    setAdWillPlayMuted() {}
    setContinuousPlayback() {}
  }

  class AdPodInfo {
    getAdPosition() {
      return 1;
    }
    getIsBumper() {
      return false;
    }
    getMaxDuration() {
      return -1;
    }
    getPodIndex() {
      return 1;
    }
    getTimeOffset() {
      return 0;
    }
    getTotalAds() {
      return 1;
    }
  }

  class Ad {
    _pi = new AdPodInfo();
    getAdId() {
      return "";
    }
    getAdPodInfo() {
      return this._pi;
    }
    getAdSystem() {
      return "";
    }
    getAdvertiserName() {
      return "";
    }
    getApiFramework() {
      return null;
    }
    getCompanionAds() {
      return [];
    }
    getContentType() {
      return "";
    }
    getCreativeAdId() {
      return "";
    }
    getCreativeId() {
      return "";
    }
    getDealId() {
      return "";
    }
    getDescription() {
      return "";
    }
    getDuration() {
      return 8.5;
    }
    getHeight() {
      return 0;
    }
    getMediaUrl() {
      return null;
    }
    getMinSuggestedDuration() {
      return -2;
    }
    getSkipTimeOffset() {
      return -1;
    }
    getSurveyUrl() {
      return null;
    }
    getTitle() {
      return "";
    }
    getTraffickingParameters() {
      return {};
    }
    getTraffickingParametersString() {
      return "";
    }
    getUiElements() {
      return [""];
    }
    getUniversalAdIdRegistry() {
      return "unknown";
    }
    getUniversalAdIds() {
      return [""];
    }
    getUniversalAdIdValue() {
      return "unknown";
    }
    getVastMediaBitrate() {
      return 0;
    }
    getVastMediaHeight() {
      return 0;
    }
    getVastMediaWidth() {
      return 0;
    }
    getWidth() {
      return 0;
    }
    getWrapperAdIds() {
      return [""];
    }
    getWrapperAdSystems() {
      return [""];
    }
    getWrapperCreativeIds() {
      return [""];
    }
    isLinear() {
      return true;
    }
  }

  class CompanionAd {
    getAdSlotId() {
      return "";
    }
    getContent() {
      return "";
    }
    getContentType() {
      return "";
    }
    getHeight() {
      return 1;
    }
    getWidth() {
      return 1;
    }
  }

  class AdError {
    getErrorCode() {
      return 0;
    }
    getInnerError() {}
    getMessage() {
      return "";
    }
    getType() {
      return "";
    }
    getVastErrorCode() {
      return 0;
    }
    toString() {
      return "";
    }
  }
  AdError.ErrorCode = {};
  AdError.Type = {};

  const isEngadget = () => {
    try {
      for (const ctx of Object.values(window.vidible._getContexts())) {
        if (ctx.getPlayer()?.div?.innerHTML.includes("www.engadget.com")) {
          return true;
        }
      }
    } catch (_) {}
    return false;
  };

  const currentAd = isEngadget() ? undefined : new Ad();

  class AdEvent {
    constructor(type) {
      this.type = type;
    }
    getAd() {
      return currentAd;
    }
    getAdData() {
      return {};
    }
  }
  AdEvent.Type = {
    AD_BREAK_READY: "adBreakReady",
    AD_BUFFERING: "adBuffering",
    AD_CAN_PLAY: "adCanPlay",
    AD_METADATA: "adMetadata",
    AD_PROGRESS: "adProgress",
    ALL_ADS_COMPLETED: "allAdsCompleted",
    CLICK: "click",
    COMPLETE: "complete",
    CONTENT_PAUSE_REQUESTED: "contentPauseRequested",
    CONTENT_RESUME_REQUESTED: "contentResumeRequested",
    DURATION_CHANGE: "durationChange",
    EXPANDED_CHANGED: "expandedChanged",
    FIRST_QUARTILE: "firstQuartile",
    IMPRESSION: "impression",
    INTERACTION: "interaction",
    LINEAR_CHANGE: "linearChange",
    LINEAR_CHANGED: "linearChanged",
    LOADED: "loaded",
    LOG: "log",
    MIDPOINT: "midpoint",
    PAUSED: "pause",
    RESUMED: "resume",
    SKIPPABLE_STATE_CHANGED: "skippableStateChanged",
    SKIPPED: "skip",
    STARTED: "start",
    THIRD_QUARTILE: "thirdQuartile",
    USER_CLOSE: "userClose",
    VIDEO_CLICKED: "videoClicked",
    VIDEO_ICON_CLICKED: "videoIconClicked",
    VIEWABLE_IMPRESSION: "viewable_impression",
    VOLUME_CHANGED: "volumeChange",
    VOLUME_MUTED: "mute",
  };

  class AdErrorEvent {
    getError() {}
    getUserRequestContext() {
      return {};
    }
  }
  AdErrorEvent.Type = {
    AD_ERROR: "adError",
  };

  const manager = new AdsManager();

  class AdsManagerLoadedEvent {
    constructor(type) {
      this.type = type;
    }
    getAdsManager() {
      return manager;
    }
    getUserRequestContext() {
      return {};
    }
  }
  AdsManagerLoadedEvent.Type = {
    ADS_MANAGER_LOADED: "adsManagerLoaded",
  };

  class CustomContentLoadedEvent {}
  CustomContentLoadedEvent.Type = {
    CUSTOM_CONTENT_LOADED: "deprecated-event",
  };

  class CompanionAdSelectionSettings {}
  CompanionAdSelectionSettings.CreativeType = {
    ALL: "All",
    FLASH: "Flash",
    IMAGE: "Image",
  };
  CompanionAdSelectionSettings.ResourceType = {
    ALL: "All",
    HTML: "Html",
    IFRAME: "IFrame",
    STATIC: "Static",
  };
  CompanionAdSelectionSettings.SizeCriteria = {
    IGNORE: "IgnoreSize",
    SELECT_EXACT_MATCH: "SelectExactMatch",
    SELECT_NEAR_MATCH: "SelectNearMatch",
  };

  class AdCuePoints {
    getCuePoints() {
      return [];
    }
  }

  class AdProgressData {}

  class UniversalAdIdInfo {
    getAdIdRegistry() {
      return "";
    }
    getAdIsValue() {
      return "";
    }
  }

  Object.assign(ima, {
    AdCuePoints,
    AdDisplayContainer,
    AdError,
    AdErrorEvent,
    AdEvent,
    AdPodInfo,
    AdProgressData,
    AdsLoader,
    AdsManager: manager,
    AdsManagerLoadedEvent,
    AdsRenderingSettings,
    AdsRequest,
    CompanionAd,
    CompanionAdSelectionSettings,
    CustomContentLoadedEvent,
    gptProxyInstance: {},
    ImaSdkSettings,
    OmidAccessMode: {
      DOMAIN: "domain",
      FULL: "full",
      LIMITED: "limited",
    },
    settings: new ImaSdkSettings(),
    UiElements: {
      AD_ATTRIBUTION: "adAttribution",
      COUNTDOWN: "countdown",
    },
    UniversalAdIdInfo,
    VERSION,
    ViewMode: {
      FULLSCREEN: "fullscreen",
      NORMAL: "normal",
    },
  });

  if (!window.google) {
    window.google = {};
  }

  window.google.ima = ima;
}
