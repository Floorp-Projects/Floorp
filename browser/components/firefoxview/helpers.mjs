/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
const loggersByName = new Map();

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  Log: "resource://gre/modules/Log.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "relativeTimeFormat", () => {
  return new Services.intl.RelativeTimeFormat(undefined, { style: "narrow" });
});

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "searchEnabledPref",
  "browser.firefox-view.search.enabled"
);

// Cutoff of 1.5 minutes + 1 second to determine what text string to display
export const NOW_THRESHOLD_MS = 91000;

// Configure logging level via this pref
export const LOGGING_PREF = "browser.tabs.firefox-view.logLevel";

export const MAX_TABS_FOR_RECENT_BROWSING = 5;

export function formatURIForDisplay(uriString) {
  return lazy.BrowserUtils.formatURIStringForDisplay(uriString);
}

export function convertTimestamp(
  timestamp,
  fluentStrings,
  _nowThresholdMs = NOW_THRESHOLD_MS
) {
  if (!timestamp) {
    // It's marginally better to show nothing instead of "53 years ago"
    return "";
  }
  const elapsed = Date.now() - timestamp;
  let formattedTime;
  if (elapsed <= _nowThresholdMs) {
    // Use a different string for very recent timestamps
    formattedTime = fluentStrings.formatValueSync(
      "firefoxview-just-now-timestamp"
    );
  } else {
    formattedTime = lazy.relativeTimeFormat.formatBestUnit(new Date(timestamp));
  }
  return formattedTime;
}

export function createFaviconElement(image, targetURI = "") {
  let favicon = document.createElement("div");
  favicon.style.backgroundImage = `url('${getImageUrl(image, targetURI)}')`;
  favicon.classList.add("favicon");
  return favicon;
}

export function getImageUrl(icon, targetURI) {
  return icon ? lazy.PlacesUIUtils.getImageURL(icon) : `page-icon:${targetURI}`;
}

/**
 * This function doesn't just copy the link to the clipboard, it creates a
 * URL object on the clipboard, so when it's pasted into an application that
 * supports it, it displays the title as a link.
 */
export function placeLinkOnClipboard(title, uri) {
  let node = {
    type: 0,
    title,
    uri,
  };

  // Copied from doCommand/placesCmd_copy in PlacesUIUtils.sys.mjs

  // This is a little hacky, but there is a lot of code in Places that handles
  // clipboard stuff, so it's easier to reuse.

  // This order is _important_! It controls how this and other applications
  // select data to be inserted based on type.
  let contents = [
    { type: lazy.PlacesUtils.TYPE_X_MOZ_URL, entries: [] },
    { type: lazy.PlacesUtils.TYPE_HTML, entries: [] },
    { type: lazy.PlacesUtils.TYPE_PLAINTEXT, entries: [] },
  ];

  contents.forEach(function (content) {
    content.entries.push(lazy.PlacesUtils.wrapNode(node, content.type));
  });

  let xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);

  function addData(type, data) {
    xferable.addDataFlavor(type);
    xferable.setTransferData(type, lazy.PlacesUtils.toISupportsString(data));
  }

  contents.forEach(function (content) {
    addData(content.type, content.entries.join(lazy.PlacesUtils.endl));
  });

  Services.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);
}

/**
 * Check the user preference to enable search functionality in Firefox View.
 *
 * @returns {boolean} The preference value.
 */
export function isSearchEnabled() {
  return lazy.searchEnabledPref;
}

/**
 * Escape special characters for regular expressions from a string.
 *
 * @param {string} string
 *   The string to sanitize.
 * @returns {string} The sanitized string.
 */
export function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/**
 * Search a tab list for items that match the given query.
 */
export function searchTabList(query, tabList) {
  const regex = RegExp(escapeRegExp(query), "i");
  return tabList.filter(
    ({ title, url }) => regex.test(title) || regex.test(url)
  );
}

/**
 * Get or create a logger, whose log-level is controlled by a pref
 *
 * @param {string} loggerName - Creating named loggers helps differentiate log messages from different
                                components or features.
 */

export function getLogger(loggerName) {
  if (!loggersByName.has(loggerName)) {
    let logger = lazy.Log.repository.getLogger(`FirefoxView.${loggerName}`);
    logger.manageLevelFromPref(LOGGING_PREF);
    logger.addAppender(
      new lazy.Log.ConsoleAppender(new lazy.Log.BasicFormatter())
    );
    loggersByName.set(loggerName, logger);
  }
  return loggersByName.get(loggerName);
}

export function escapeHtmlEntities(text) {
  return (text || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}
