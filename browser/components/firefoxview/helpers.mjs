/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "relativeTimeFormat", () => {
  return new Services.intl.RelativeTimeFormat(undefined, { style: "narrow" });
});

// Cutoff of 1.5 minutes + 1 second to determine what text string to display
export const NOW_THRESHOLD_MS = 91000;

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

export function onToggleContainer(detailsContainer) {
  const doc = detailsContainer.ownerDocument;
  // Ignore early `toggle` events, which may either be fired because the
  // UI sections update visibility on component connected (based on persisted
  // UI state), or because <details> elements fire `toggle` events when added
  // to the DOM with the "open" attribute set. In either case, we don't want
  // to record telemetry as these events aren't the result of user action.
  if (doc.readyState != "complete") {
    return;
  }

  const isOpen = detailsContainer.open;
  const isTabPickup = detailsContainer.id === "tab-pickup-container";

  const newFluentString = isOpen
    ? "firefoxview-collapse-button-hide"
    : "firefoxview-collapse-button-show";

  doc.l10n.setAttributes(
    detailsContainer.querySelector(".twisty"),
    newFluentString
  );

  if (isTabPickup) {
    Services.telemetry.recordEvent(
      "firefoxview",
      "tab_pickup_open",
      "tabs",
      isOpen.toString()
    );
    Services.prefs.setBoolPref(
      "browser.tabs.firefox-view.ui-state.tab-pickup.open",
      isOpen
    );
  } else {
    Services.telemetry.recordEvent(
      "firefoxview",
      "closed_tabs_open",
      "tabs",
      isOpen.toString()
    );
    Services.prefs.setBoolPref(
      "browser.tabs.firefox-view.ui-state.recently-closed-tabs.open",
      isOpen
    );
  }
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
