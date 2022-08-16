/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
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
  const relativeTimeFormat = new Services.intl.RelativeTimeFormat(
    undefined,
    {}
  );
  const elapsed = Date.now() - timestamp;
  let formattedTime;
  if (elapsed <= _nowThresholdMs) {
    // Use a different string for very recent timestamps
    formattedTime = fluentStrings.formatValueSync(
      "firefoxview-just-now-timestamp"
    );
  } else {
    formattedTime = relativeTimeFormat.formatBestUnit(new Date(timestamp));
  }
  return formattedTime;
}

export function createFaviconElement(image) {
  const imageUrl = image
    ? lazy.PlacesUIUtils.getImageURL(image)
    : "chrome://global/skin/icons/defaultFavicon.svg";
  let favicon = document.createElement("div");

  favicon.style.backgroundImage = `url('${imageUrl}')`;
  favicon.classList.add("favicon");
  return favicon;
}

export function toggleContainer(collapsibleButton, collapsibleContainer) {
  const arrowDown = "arrow-down";
  collapsibleButton.classList.toggle(arrowDown);
  let isHidden = collapsibleButton.classList.contains(arrowDown);

  const newFluentString = `${
    !isHidden
      ? "firefoxview-collapse-button-hide"
      : "firefoxview-collapse-button-show"
  }`;
  collapsibleButton.setAttribute("data-l10n-id", newFluentString);

  collapsibleButton.setAttribute("aria-expanded", !isHidden);
  collapsibleButton.setAttribute("data-l10n-id", newFluentString);
  collapsibleContainer.hidden = isHidden;

  Services.telemetry.recordEvent(
    "firefoxview",
    collapsibleButton.id === "collapsible-synced-tabs-button"
      ? "tab_pickup_open"
      : "closed_tabs_open",
    "tabs",
    (!isHidden).toString()
  );
}
