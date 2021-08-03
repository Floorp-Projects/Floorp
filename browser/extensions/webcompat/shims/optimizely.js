/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 1714431 - Shim Optimizely
 *
 * This shim stubs out window.optimizely for those sites which
 * break when it is not successfully loaded.
 */

if (!window.optimizely?.state) {
  const behavior = {
    query: () => [],
  };

  const dcp = {
    getAttributeValue() {},
    waitForAttributeValue: () => Promise.resolve(),
  };

  const data = {
    accountId: "",
    audiences: {},
    campaigns: {},
    clientName: "js",
    clientVersion: "0.166.0",
    dcpServiceId: null,
    events: {},
    experiments: {},
    groups: {},
    pages: {},
    projectId: "",
    revision: "",
    snippetId: null,
    variations: {},
  };

  const state = {
    getActivationId() {},
    getActiveExperimentIds() {
      return [];
    },
    getCampaignStateLists() {},
    getCampaignStates() {},
    getDecisionObject() {},
    getDecisionString() {},
    getExperimentStates() {},
    getPageStates() {},
    getRedirectInfo() {},
    getVariationMap() {},
    isGlobalHoldback() {},
  };

  const utils = {
    Promise: window.Promise,
    observeSelector() {},
    poll() {},
    waitForElement() {},
    waitUntil() {},
  };

  const visitorId = {
    randomId: "",
  };

  let browserVersion = "";
  try {
    browserVersion = navigator.userAgent.match(/rv:(.*)\)/)[1];
  } catch (_) {}

  const visitor = {
    browserId: "ff",
    browserVersion,
    currentTimestamp: Date.now(),
    custom: {},
    customBehavior: {},
    device: "desktop",
    device_type: "desktop_laptop",
    events: [],
    first_session: true,
    offset: 240,
    referrer: null,
    source_type: "direct",
    visitorId,
  };

  window.optimizely = {
    data: {
      note: "Obsolete, use optimizely.get('data') instead",
    },
    get(e) {
      switch (e) {
        case "behavior":
          return behavior;
        case "data":
          return data;
        case "dcp":
          return dcp;
        case "jquery":
          throw new Error("jQuery not implemented");
        case "session":
          return undefined;
        case "state":
          return state;
        case "utils":
          return utils;
        case "visitor":
          return visitor;
        case "visitor_id":
          return visitorId;
      }
      return undefined;
    },
    initialized: true,
    push() {},
    state: {},
  };
}
