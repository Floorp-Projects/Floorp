/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1600538 - Shim Google Publisher Tags
 */

"use strict";

if (!window.googletag?.apiReady) {
  const noopfn = function() {};
  const noopthisfn = function() {
    return this;
  };
  const noopnullfn = function() {
    return null;
  };
  const nooparrayfn = function() {
    return [];
  };
  const noopstrfn = function() {
    return "";
  };

  function newPassbackSlot() {
    return {
      display: noopfn,
      get: noopnullfn,
      set: noopthisfn,
      setClickUrl: noopthisfn,
      setTagForChildDirectedTreatment: noopthisfn,
      setTargeting: noopthisfn,
      updateTargetingFromMap: noopthisfn,
    };
  }

  function display(id) {
    const parent = document.getElementById(id);
    if (parent) {
      parent.appendChild(document.createElement("div"));
    }
  }

  const companionAdsService = {
    addEventListener: noopthisfn,
    enableSyncLoading: noopfn,
    setRefreshUnfilledSlots: noopfn,
  };

  const contentService = {
    addEventListener: noopthisfn,
    setContent: noopfn,
  };

  const pubadsService = {
    addEventListener: noopthisfn,
    clear: noopfn,
    clearCategoryExclusions: noopthisfn,
    clearTagForChildDirectedTreatment: noopthisfn,
    clearTargeting: noopthisfn,
    collapseEmptyDivs: noopfn,
    defineOutOfPagePassback: () => newPassbackSlot(),
    definePassback: () => newPassbackSlot(),
    disableInitialLoad: noopfn,
    display,
    enableAsyncRendering: noopfn,
    enableSingleRequest: noopfn,
    enableSyncRendering: noopfn,
    enableVideoAds: noopfn,
    get: noopnullfn,
    getAttributeKeys: nooparrayfn,
    getTargeting: noopfn,
    getTargetingKeys: nooparrayfn,
    getSlots: nooparrayfn,
    refresh: noopfn,
    set: noopthisfn,
    setCategoryExclusion: noopthisfn,
    setCentering: noopfn,
    setCookieOptions: noopthisfn,
    setForceSafeFrame: noopthisfn,
    setLocation: noopthisfn,
    setPublisherProvidedId: noopthisfn,
    setRequestNonPersonalizedAds: noopthisfn,
    setSafeFrameConfig: noopthisfn,
    setTagForChildDirectedTreatment: noopthisfn,
    setTargeting: noopthisfn,
    setVideoContent: noopthisfn,
    updateCorrelator: noopfn,
  };

  function newSizeMappingBuilder() {
    return {
      addSize: noopthisfn,
      build: noopnullfn,
    };
  }

  function newSlot() {
    return {
      addService: noopthisfn,
      clearCategoryExclusions: noopthisfn,
      clearTargeting: noopthisfn,
      defineSizeMapping: noopthisfn,
      get: noopnullfn,
      getAdUnitPath: nooparrayfn,
      getAttributeKeys: nooparrayfn,
      getCategoryExclusions: nooparrayfn,
      getDomId: noopstrfn,
      getSlotElementId: noopstrfn,
      getSlotId: noopthisfn,
      getTargeting: nooparrayfn,
      getTargetingKeys: nooparrayfn,
      set: noopthisfn,
      setCategoryExclusion: noopthisfn,
      setClickUrl: noopthisfn,
      setCollapseEmptyDiv: noopthisfn,
      setTargeting: noopthisfn,
    };
  }

  let gt = window.googletag;
  if (!gt) {
    gt = window.googletag = {};
  }

  for (const [key, value] of Object.entries({
    apiReady: true,
    companionAds: () => companionAdsService,
    content: () => contentService,
    defineOutOfPageSlot: () => newSlot(),
    defineSlot: () => newSlot(),
    destroySlots: noopfn,
    disablePublisherConsole: noopfn,
    display,
    enableServices: noopfn,
    getVersion: noopstrfn,
    pubads: () => pubadsService,
    pubabsReady: true,
    setAdIframeTitle: noopfn,
    sizeMapping: () => newSizeMappingBuilder(),
  })) {
    gt[key] = value;
  }

  function runCmd(fn) {
    try {
      fn();
    } catch (_) {}
    return 1;
  }

  const cmds = gt.cmd;
  const newCmd = [];
  newCmd.push = runCmd;
  gt.cmd = newCmd;

  for (const cmd of cmds) {
    runCmd(cmd);
  }
}
