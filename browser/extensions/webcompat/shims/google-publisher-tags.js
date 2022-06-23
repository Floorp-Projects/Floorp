/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713685 - Shim Google Publisher Tags
 *
 * Many sites rely on googletag to place content or drive ad bidding,
 * and will experience major breakage if it is blocked. This shim provides
 * enough of the API's frame To mitigate much of that breakage.
 */

if (window.googletag?.apiReady === undefined) {
  const version = "2021050601";

  const noopthisfn = function() {
    return this;
  };

  const slots = new Map();
  const slotsById = new Map();
  const slotsPerPath = new Map();

  const displayedSlots = new Set();
  const refreshedSlots = new Set();
  const eventCallbacks = new Map();

  const fireSlotEvent = (name, slot) => {
    return new Promise(resolve => {
      requestAnimationFrame(() => {
        const size = [0, 0];
        for (const cb of eventCallbacks.get(name) || []) {
          cb({ isEmpty: true, size, slot });
        }
        resolve();
      });
    });
  };

  const recreateIframeForSlot = slot => {
    const eid = `google_ads_iframe_${slot.getId()}`;
    document.getElementById(eid)?.remove();
    const node = document.getElementById(slot.getSlotElementId());
    if (node) {
      const f = document.createElement("iframe");
      f.id = eid;
      f.srcdoc = "<body></body>";
      f.style =
        "position:absolute; width:0; height:0; left:0; right:0; z-index:-1; border:0";
      f.setAttribute("width", 0);
      f.setAttribute("height", 0);
      node.appendChild(f);
    }
  };

  const emptySlotElement = slot => {
    const node = document.getElementById(slot.getSlotElementId());
    while (node?.lastChild) {
      node.lastChild.remove();
    }
  };

  const callbackIfSlotReady = async id => {
    const slot = slotsById.get(id);
    if (!slot || !refreshedSlots.has(id) || !displayedSlots.has(id)) {
      return;
    }
    emptySlotElement(slot);
    recreateIframeForSlot(slot);
    await fireSlotEvent("slotRenderEnded", slot);
    await fireSlotEvent("slotRequested", slot);
    await fireSlotEvent("slotResponseReceived", slot);
    await fireSlotEvent("slotOnload", slot);
    await fireSlotEvent("impressionViewable", slot);
  };

  const display = id => {
    const parent = document.getElementById(id);
    if (parent) {
      parent.appendChild(document.createElement("div"));
    }

    if (slotsById.has(id)) {
      displayedSlots.add(id);
      callbackIfSlotReady(id);
    }
  };

  const addEventListener = function(name, listener) {
    if (!eventCallbacks.has(name)) {
      eventCallbacks.set(name, new Set());
    }
    eventCallbacks.get(name).add(listener);
    return this;
  };

  const removeEventListener = function(name, listener) {
    if (eventCallbacks.has(name)) {
      return eventCallbacks.get(name).delete(listener);
    }
    return false;
  };

  const companionAdsService = {
    addEventListener,
    display,
    enable() {},
    fillSlot() {},
    getAttributeKeys: () => [],
    getDisplayAdsCorrelator: () => "",
    getName: () => "companion_ads",
    getSlotIdMap: () => {
      return {};
    },
    getSlots: () => [],
    getVideoStreamCorrelator() {},
    isRoadblockingSupported: () => false,
    isSlotAPersistentRoadblock: () => false,
    notifyUnfilledSlots() {},
    onImplementationLoaded() {},
    refreshAllSlots() {},
    removeEventListener,
    set() {},
    setRefreshUnfilledSlots() {},
    setVideoSession() {},
    slotRenderEnded() {},
  };

  const contentService = {
    addEventListener,
    setContent() {},
    removeEventListener,
  };

  const getTargetingValue = v => {
    if (typeof v === "string") {
      return [v];
    }
    try {
      return [Array.prototype.flat.call(v)[0]];
    } catch (_) {}
    return [];
  };

  const updateTargeting = (targeting, map) => {
    if (typeof map === "object") {
      const entries = Object.entries(map || {});
      for (const [k, v] of entries) {
        targeting.set(k, getTargetingValue(v));
      }
    }
  };

  const newSlot = (adUnitPath, size, opt_div) => {
    if (slotsById.has(opt_div)) {
      document.getElementById(opt_div)?.remove();
      return undefined;
    }
    const attributes = new Map();
    const targeting = new Map();
    const exclusions = new Set();
    const response = {
      advertiserId: undefined,
      campaignId: undefined,
      creativeId: undefined,
      creativeTemplateId: undefined,
      lineItemId: undefined,
    };
    const sizes = [
      {
        getHeight: () => 2,
        getWidth: () => 2,
      },
    ];
    const num = (slotsPerPath.get(adUnitPath) || 0) + 1;
    slotsPerPath.set(adUnitPath, num);
    const id = `${adUnitPath}_${num}`;
    let clickUrl = "";
    let collapseEmptyDiv = null;
    let services = new Set();
    const slot = {
      addService(e) {
        services.add(e);
        return slot;
      },
      clearCategoryExclusions: noopthisfn,
      clearTargeting(k) {
        if (k === undefined) {
          targeting.clear();
        } else {
          targeting.delete(k);
        }
      },
      defineSizeMapping: noopthisfn,
      get: k => attributes.get(k),
      getAdUnitPath: () => adUnitPath,
      getAttributeKeys: () => Array.from(attributes.keys()),
      getCategoryExclusions: () => Array.from(exclusions),
      getClickUrl: () => clickUrl,
      getCollapseEmptyDiv: () => collapseEmptyDiv,
      getContentUrl: () => "",
      getDivStartsCollapsed: () => null,
      getDomId: () => opt_div,
      getEscapedQemQueryId: () => "",
      getFirstLook: () => 0,
      getId: () => id,
      getHtml: () => "",
      getName: () => id,
      getOutOfPage: () => false,
      getResponseInformation: () => response,
      getServices: () => Array.from(services),
      getSizes: () => sizes,
      getSlotElementId: () => opt_div,
      getSlotId: () => slot,
      getTargeting: k => targeting.get(k) || gTargeting.get(k) || [],
      getTargetingKeys: () =>
        Array.from(
          new Set(Array.of(...gTargeting.keys(), ...targeting.keys()))
        ),
      getTargetingMap: () =>
        Object.assign(
          Object.fromEntries(gTargeting.entries()),
          Object.fromEntries(targeting.entries())
        ),
      set(k, v) {
        attributes.set(k, v);
        return slot;
      },
      setCategoryExclusion(e) {
        exclusions.add(e);
        return slot;
      },
      setClickUrl(u) {
        clickUrl = u;
        return slot;
      },
      setCollapseEmptyDiv(v) {
        collapseEmptyDiv = !!v;
        return slot;
      },
      setSafeFrameConfig: noopthisfn,
      setTagForChildDirectedTreatment: noopthisfn,
      setTargeting(k, v) {
        targeting.set(k, getTargetingValue(v));
        return slot;
      },
      toString: () => id,
      updateTargetingFromMap(map) {
        updateTargeting(targeting, map);
        return slot;
      },
    };
    slots.set(adUnitPath, slot);
    slotsById.set(opt_div, slot);
    return slot;
  };

  let initialLoadDisabled = false;

  const gTargeting = new Map();
  const gAttributes = new Map();

  let imaContent = { vid: "", cmsid: "" };
  let videoContent = { vid: "", cmsid: "" };

  const pubadsService = {
    addEventListener,
    clear() {},
    clearCategoryExclusions: noopthisfn,
    clearTagForChildDirectedTreatment: noopthisfn,
    clearTargeting(k) {
      if (k === undefined) {
        gTargeting.clear();
      } else {
        gTargeting.delete(k);
      }
    },
    collapseEmptyDivs() {},
    defineOutOfPagePassback: (a, o) => newSlot(a, 0, o),
    definePassback: (a, s, o) => newSlot(a, s, o),
    disableInitialLoad() {
      initialLoadDisabled = true;
      return this;
    },
    display,
    enable() {},
    enableAsyncRendering() {},
    enableLazyLoad() {},
    enableSingleRequest() {},
    enableSyncRendering() {},
    enableVideoAds() {},
    forceExperiment() {},
    get: k => gAttributes.get(k),
    getAttributeKeys: () => Array.from(gAttributes.keys()),
    getCorrelator() {},
    getImaContent: () => imaContent,
    getName: () => "publisher_ads",
    getSlots: () => Array.from(slots.values()),
    getSlotIdMap() {
      const map = {};
      slots.values().forEach(s => {
        map[s.getId()] = s;
      });
      return map;
    },
    getTagSessionCorrelator() {},
    getTargeting: k => gTargeting.get(k) || [],
    getTargetingKeys: () => Array.from(gTargeting.keys()),
    getTargetingMap: () => Object.fromEntries(gTargeting.entries()),
    getVersion: () => version,
    getVideoContent: () => videoContent,
    isInitialLoadDisabled: () => initialLoadDisabled,
    isSRA: () => false,
    markAsAmp() {},
    refresh(slts) {
      if (!slts) {
        slts = slots.values();
      } else if (!Array.isArray(slts)) {
        slts = [slts];
      }
      for (const slot of slts) {
        if (slot) {
          try {
            const id = slot.getSlotElementId();
            displayedSlots.add(id);
            refreshedSlots.add(id);
            callbackIfSlotReady(id);
          } catch (e) {
            console.error(e);
          }
        }
      }
    },
    removeEventListener,
    set(k, v) {
      gAttributes[k] = v;
      return this;
    },
    setCategoryExclusion: noopthisfn,
    setCentering() {},
    setCookieOptions: noopthisfn,
    setCorrelator: noopthisfn,
    setForceSafeFrame: noopthisfn,
    setImaContent(vid, cmsid) {
      imaContent = { vid, cmsid };
      return this;
    },
    setLocation: noopthisfn,
    setPrivacySettings: noopthisfn,
    setPublisherProvidedId: noopthisfn,
    setRequestNonPersonalizedAds: noopthisfn,
    setSafeFrameConfig: noopthisfn,
    setTagForChildDirectedTreatment: noopthisfn,
    setTagForUnderAgeOfConsent: noopthisfn,
    setTargeting(k, v) {
      gTargeting.set(k, getTargetingValue(v));
      return this;
    },
    setVideoContent(vid, cmsid) {
      videoContent = { vid, cmsid };
      return this;
    },
    updateCorrelator() {},
    updateTargetingFromMap(map) {
      updateTargeting(gTargeting, map);
      return this;
    },
  };

  const newSizeMappingBuilder = () => {
    return {
      addSize: noopthisfn,
      build: () => null,
    };
  };

  let gt = window.googletag;
  if (!gt) {
    gt = window.googletag = {};
  }

  Object.assign(gt, {
    apiReady: true,
    companionAds: () => companionAdsService,
    content: () => contentService,
    defineOutOfPageSlot: (a, o) => newSlot(a, 0, o),
    defineSlot: (a, s, o) => newSlot(a, s, o),
    destroySlots() {
      slots.clear();
      slotsById.clear();
    },
    disablePublisherConsole() {},
    display,
    enableServices() {},
    enums: {
      OutOfPageFormat: {
        BOTTOM_ANCHOR: 3,
        INTERSTITIAL: 5,
        REWARDED: 4,
        TOP_ANCHOR: 2,
      },
    },
    getVersion: () => version,
    pubads: () => pubadsService,
    pubadsReady: true,
    setAdIframeTitle() {},
    sizeMapping: () => newSizeMappingBuilder(),
  });

  const run = function(fn) {
    if (typeof fn === "function") {
      try {
        fn.call(window);
      } catch (e) {
        console.error(e);
      }
    }
  };

  const cmds = gt.cmd || [];
  const newCmd = [];
  newCmd.push = run;
  gt.cmd = newCmd;

  for (const cmd of cmds) {
    run(cmd);
  }
}
