/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Ci, Cr } = require("chrome");
const Services = require("Services");

const {
  wildcardToRegExp,
} = require("devtools/server/actors/network-monitor/utils/wildcard-to-regexp");

loader.lazyRequireGetter(
  this,
  "NetworkHelper",
  "devtools/shared/webconsole/network-helper"
);

loader.lazyGetter(this, "tpFlagsMask", () => {
  const trackingProtectionLevel2Enabled = Services.prefs
    .getStringPref("urlclassifier.trackingTable")
    .includes("content-track-digest256");

  return trackingProtectionLevel2Enabled
    ? ~Ci.nsIClassifiedChannel.CLASSIFIED_ANY_BASIC_TRACKING &
        ~Ci.nsIClassifiedChannel.CLASSIFIED_ANY_STRICT_TRACKING
    : ~Ci.nsIClassifiedChannel.CLASSIFIED_ANY_BASIC_TRACKING &
        Ci.nsIClassifiedChannel.CLASSIFIED_ANY_STRICT_TRACKING;
});

/**
 * Convert a nsIContentPolicy constant to a display string
 */
const LOAD_CAUSE_STRINGS = {
  [Ci.nsIContentPolicy.TYPE_INVALID]: "invalid",
  [Ci.nsIContentPolicy.TYPE_OTHER]: "other",
  [Ci.nsIContentPolicy.TYPE_SCRIPT]: "script",
  [Ci.nsIContentPolicy.TYPE_IMAGE]: "img",
  [Ci.nsIContentPolicy.TYPE_STYLESHEET]: "stylesheet",
  [Ci.nsIContentPolicy.TYPE_OBJECT]: "object",
  [Ci.nsIContentPolicy.TYPE_DOCUMENT]: "document",
  [Ci.nsIContentPolicy.TYPE_SUBDOCUMENT]: "subdocument",
  [Ci.nsIContentPolicy.TYPE_PING]: "ping",
  [Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST]: "xhr",
  [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "objectSubdoc",
  [Ci.nsIContentPolicy.TYPE_DTD]: "dtd",
  [Ci.nsIContentPolicy.TYPE_FONT]: "font",
  [Ci.nsIContentPolicy.TYPE_MEDIA]: "media",
  [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "websocket",
  [Ci.nsIContentPolicy.TYPE_CSP_REPORT]: "csp",
  [Ci.nsIContentPolicy.TYPE_XSLT]: "xslt",
  [Ci.nsIContentPolicy.TYPE_BEACON]: "beacon",
  [Ci.nsIContentPolicy.TYPE_FETCH]: "fetch",
  [Ci.nsIContentPolicy.TYPE_IMAGESET]: "imageset",
  [Ci.nsIContentPolicy.TYPE_WEB_MANIFEST]: "webManifest",
};

exports.causeTypeToString = function(
  causeType,
  loadFlags,
  internalContentPolicyType
) {
  let prefix = "";
  if (
    (causeType == Ci.nsIContentPolicy.TYPE_IMAGESET ||
      internalContentPolicyType == Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE) &&
    loadFlags & Ci.nsIRequest.LOAD_BACKGROUND
  ) {
    prefix = "lazy-";
  }

  return prefix + LOAD_CAUSE_STRINGS[causeType] || "unknown";
};

exports.stringToCauseType = function(value) {
  return Object.keys(LOAD_CAUSE_STRINGS).find(
    key => LOAD_CAUSE_STRINGS[key] === value
  );
};

function isChannelFromSystemPrincipal(channel) {
  let principal = null;
  let browsingContext = channel.loadInfo.browsingContext;
  if (!browsingContext) {
    const topFrame = NetworkHelper.getTopFrameForRequest(channel);
    if (topFrame) {
      browsingContext = topFrame.browsingContext;
    } else {
      // Fallback to the triggering principal when browsingContext and topFrame is null
      // e.g some chrome requests
      principal = channel.loadInfo.triggeringPrincipal;
    }
  }

  // When in the parent process, we can get the documentPrincipal from the
  // WindowGlobal which is available on the BrowsingContext
  if (!principal) {
    principal = CanonicalBrowsingContext.isInstance(browsingContext)
      ? browsingContext.currentWindowGlobal.documentPrincipal
      : browsingContext.window.document.nodePrincipal;
  }
  return principal.isSystemPrincipal;
}

/**
 * Get the browsing context id for the channel.
 *
 * @param {*} channel
 * @returns {number}
 */
exports.getChannelBrowsingContextID = function(channel) {
  if (channel.loadInfo.browsingContextID) {
    return channel.loadInfo.browsingContextID;
  }
  // At least WebSocket channel aren't having a browsingContextID set on their loadInfo
  // We fallback on top frame element, which works, but will be wrong for WebSocket
  // in same-process iframes...
  const topFrame = NetworkHelper.getTopFrameForRequest(channel);
  // topFrame is typically null for some chrome requests like favicons
  if (topFrame && topFrame.browsingContext) {
    return topFrame.browsingContext.id;
  }
  return null;
};

/**
 * Get the innerWindowId for the channel.
 *
 * @param {*} channel
 * @returns {number}
 */
exports.getChannelInnerWindowId = function(channel) {
  if (channel.loadInfo.innerWindowID) {
    return channel.loadInfo.innerWindowID;
  }
  // At least WebSocket channel aren't having a browsingContextID set on their loadInfo
  // We fallback on top frame element, which works, but will be wrong for WebSocket
  // in same-process iframes...
  const topFrame = NetworkHelper.getTopFrameForRequest(channel);
  // topFrame is typically null for some chrome requests like favicons
  if (topFrame?.browsingContext?.currentWindowGlobal) {
    return topFrame.browsingContext.currentWindowGlobal.innerWindowId;
  }
  return null;
};

/**
 * Does this channel represent a Preload request.
 *
 * @param {*} channel
 * @returns {boolean}
 */
exports.isPreloadRequest = function(channel) {
  const type = channel.loadInfo.internalContentPolicyType;
  return (
    type == Ci.nsIContentPolicy.TYPE_INTERNAL_SCRIPT_PRELOAD ||
    type == Ci.nsIContentPolicy.TYPE_INTERNAL_MODULE_PRELOAD ||
    type == Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_PRELOAD ||
    type == Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET_PRELOAD ||
    type == Ci.nsIContentPolicy.TYPE_INTERNAL_FONT_PRELOAD
  );
};

/**
 * Creates a network event based on the channel.
 *
 * @param {*} channel
 * @return {Object} event - The network event
 */
exports.createNetworkEvent = function(
  channel,
  {
    timestamp,
    fromCache,
    fromServiceWorker,
    extraStringData,
    blockedReason,
    blockingExtension = null,
    blockedURLs = [],
    saveRequestAndResponseBodies = false,
  }
) {
  channel.QueryInterface(Ci.nsIPrivateBrowsingChannel);

  const event = {};
  event.method = channel.requestMethod;
  event.channelId = channel.channelId;
  event.isFromSystemPrincipal = isChannelFromSystemPrincipal(channel);
  event.browsingContextID = this.getChannelBrowsingContextID(channel);
  event.innerWindowId = this.getChannelInnerWindowId(channel);
  event.url = channel.URI.spec;
  event.private = channel.isChannelPrivate;
  event.headersSize = extraStringData ? extraStringData.length : 0;
  event.startedDateTime = (timestamp
    ? new Date(Math.round(timestamp / 1000))
    : new Date()
  ).toISOString();
  event.fromCache = fromCache;
  event.fromServiceWorker = fromServiceWorker;
  // Only consider channels classified as level-1 to be trackers if our preferences
  // would not cause such channels to be blocked in strict content blocking mode.
  // Make sure the value produced here is a boolean.
  if (channel instanceof Ci.nsIClassifiedChannel) {
    event.isThirdPartyTrackingResource = !!(
      channel.isThirdPartyTrackingResource() &&
      (channel.thirdPartyClassificationFlags & tpFlagsMask) == 0
    );
  }
  const referrerInfo = channel.referrerInfo;
  event.referrerPolicy = referrerInfo
    ? referrerInfo.getReferrerPolicyString()
    : "";

  if (channel instanceof Ci.nsISupportsPriority) {
    event.priority = channel.priority;
  }

  // Determine the cause and if this is an XHR request.
  let causeType = Ci.nsIContentPolicy.TYPE_OTHER;
  let causeUri = null;

  if (channel.loadInfo) {
    causeType = channel.loadInfo.externalContentPolicyType;
    const { loadingPrincipal } = channel.loadInfo;
    if (loadingPrincipal) {
      causeUri = loadingPrincipal.spec;
    }
  }

  // Show the right WebSocket URL in case of WS channel.
  if (channel.notificationCallbacks) {
    let wsChannel = null;
    try {
      wsChannel = channel.notificationCallbacks.QueryInterface(
        Ci.nsIWebSocketChannel
      );
    } catch (e) {
      // Not all channels implement nsIWebSocketChannel.
    }
    if (wsChannel) {
      event.url = wsChannel.URI.spec;
      event.serial = wsChannel.serial;
    }
  }

  event.cause = {
    type: this.causeTypeToString(
      causeType,
      channel.loadFlags,
      channel.loadInfo.internalContentPolicyType
    ),
    loadingDocumentUri: causeUri,
    stacktrace: undefined,
  };

  event.isXHR =
    causeType === Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST ||
    causeType === Ci.nsIContentPolicy.TYPE_FETCH;

  // Determine the HTTP version.
  const httpVersionMaj = {};
  const httpVersionMin = {};

  channel.QueryInterface(Ci.nsIHttpChannelInternal);
  channel.getRequestVersion(httpVersionMaj, httpVersionMin);

  event.httpVersion =
    "HTTP/" + httpVersionMaj.value + "." + httpVersionMin.value;

  event.discardRequestBody = !saveRequestAndResponseBodies;
  event.discardResponseBody = !saveRequestAndResponseBodies;

  // Check the request URL with ones manually blocked by the user in DevTools.
  // If it's meant to be blocked, we cancel the request and annotate the event.
  if (!blockedReason) {
    if (blockedReason !== undefined) {
      // We were definitely blocked, but the blocker didn't say why.
      event.blockedReason = "unknown";
    } else if (blockedURLs.some(url => wildcardToRegExp(url).test(event.url))) {
      channel.cancel(Cr.NS_BINDING_ABORTED);
      event.blockedReason = "devtools";
    }
  } else {
    event.blockedReason = blockedReason;
    if (blockingExtension) {
      event.blockingExtension = blockingExtension;
    }
  }

  // isNavigationRequest is true for the one request used to load a new top level document
  // of a given tab, or top level window. It will typically be false for navigation requests
  // of iframes, i.e. the request loading another document in an iframe.
  event.isNavigationRequest =
    channel.isMainDocumentChannel && channel.loadInfo.isTopLevelLoad;

  return event;
};

/**
 * For a given channel, with its associated http activity object,
 * fetch the request's headers and cookies.
 * This data is passed to the owner, i.e. the NetworkEventActor,
 * so that the frontend can later fetch it via getRequestHeaders/getRequestCookies.
 *
 * @param {*} channel
 * @param {*} owner - The network event actor
 * @param {Object} extraStringData - The uncached response headers.
 */
exports.fetchRequestHeadersAndCookies = function(
  channel,
  owner,
  { extraStringData = "" }
) {
  const headers = [];
  let cookies = [];
  let cookieHeader = null;

  // Copy the request header data.
  channel.visitRequestHeaders({
    visitHeader: function(name, value) {
      if (name == "Cookie") {
        cookieHeader = value;
      }
      headers.push({ name: name, value: value });
    },
  });

  if (cookieHeader) {
    cookies = NetworkHelper.parseCookieHeader(cookieHeader);
  }

  owner.addRequestHeaders(headers, extraStringData);
  owner.addRequestCookies(cookies);
};

/**
 * Check if a given network request should be logged by a network monitor
 * based on the specified filters.
 *
 * @param nsIHttpChannel channel
 *        Request to check.
 * @param filters
 *        NetworkObserver filters to match against. An object with one of the following attributes:
 *        - sessionContext: When inspecting requests from the parent process, pass the WatcherActor's session context.
 *          This helps know what is the overall debugged scope.
 *          See watcher actor constructor for more info.
 *        - targetActor: When inspecting requests from the content process, pass the WindowGlobalTargetActor.
 *          This helps know what exact subset of request we should accept.
 *          This is especially useful to behave correctly regarding EFT, where we should include or not
 *          iframes requests.
 *        - browserId, addonId, window: All these attributes are legacy.
 *          Only browserId attribute is still used by the legacy WebConsoleActor startListener API.
 * @return boolean
 *         True if the network request should be logged, false otherwise.
 */
function matchRequest(channel, filters) {
  // NetworkEventWatcher should now pass a session context for the parent process codepath
  if (filters.sessionContext) {
    const { type } = filters.sessionContext;
    if (type == "all") {
      return true;
    }

    // Ignore requests from chrome or add-on code when we don't monitor the whole browser
    if (
      channel.loadInfo?.loadingDocument === null &&
      (channel.loadInfo.loadingPrincipal ===
        Services.scriptSecurityManager.getSystemPrincipal() ||
        channel.loadInfo.isInDevToolsContext)
    ) {
      return false;
    }

    if (type == "browser-element") {
      if (!channel.loadInfo.browsingContext) {
        const topFrame = NetworkHelper.getTopFrameForRequest(channel);
        // `topFrame` is typically null for some chrome requests like favicons
        // And its `browsingContext` attribute might be null if the request happened
        // while the tab is being closed.
        return (
          topFrame?.browsingContext?.browserId ==
          filters.sessionContext.browserId
        );
      }
      return (
        channel.loadInfo.browsingContext.browserId ==
        filters.sessionContext.browserId
      );
    }
    if (type == "webextension") {
      return (
        channel?.loadInfo.loadingPrincipal.addonId ===
        filters.sessionContext.addonId
      );
    }
    throw new Error("Unsupported session context type: " + type);
  }

  // NetworkEventContentWatcher and NetworkEventStackTraces pass a target actor instead, from the content processes
  // Because of EFT, we can't use session context as we have to know what exact windows the target actor covers.
  if (filters.targetActor) {
    // Bug 1769982 the target actor might be destroying and accessing windows will throw.
    // Ignore all further request when this happens.
    let windows;
    try {
      windows = filters.targetActor.windows;
    } catch (e) {
      return false;
    }
    const win = NetworkHelper.getWindowForRequest(channel);
    return windows.includes(win);
  }

  // This is fallback code for the legacy WebConsole.startListeners codepath,
  // which may still pass individual browserId/window/addonId attributes.
  // This should be removable once we drop the WebConsole codepath for network events
  // (bug 1721592 and followups)
  return legacyMatchRequest(channel, filters);
}

function legacyMatchRequest(channel, filters) {
  // Log everything if no filter is specified
  if (!filters.browserId && !filters.window && !filters.addonId) {
    return true;
  }

  // Ignore requests from chrome or add-on code when we are monitoring
  // content.
  if (
    channel.loadInfo &&
    channel.loadInfo.loadingDocument === null &&
    (channel.loadInfo.loadingPrincipal ===
      Services.scriptSecurityManager.getSystemPrincipal() ||
      channel.loadInfo.isInDevToolsContext)
  ) {
    return false;
  }

  if (filters.window) {
    let win = NetworkHelper.getWindowForRequest(channel);
    if (filters.matchExactWindow) {
      return win == filters.window;
    }

    // Since frames support, this.window may not be the top level content
    // frame, so that we can't only compare with win.top.
    while (win) {
      if (win == filters.window) {
        return true;
      }
      if (win.parent == win) {
        break;
      }
      win = win.parent;
    }
    return false;
  }

  if (filters.browserId) {
    const topFrame = NetworkHelper.getTopFrameForRequest(channel);
    // `topFrame` is typically null for some chrome requests like favicons
    // And its `browsingContext` attribute might be null if the request happened
    // while the tab is being closed.
    if (topFrame?.browsingContext?.browserId == filters.browserId) {
      return true;
    }

    // If we couldn't get the top frame BrowsingContext from the loadContext,
    // look for it on channel.loadInfo instead.
    if (
      channel.loadInfo &&
      channel.loadInfo.browsingContext &&
      channel.loadInfo.browsingContext.browserId == filters.browserId
    ) {
      return true;
    }
  }

  if (
    filters.addonId &&
    channel?.loadInfo.loadingPrincipal.addonId === filters.addonId
  ) {
    return true;
  }

  return false;
}
exports.matchRequest = matchRequest;
