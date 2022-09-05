/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "relativeTimeFormat", () => {
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

export function createFaviconElement(image) {
  const imageUrl = image
    ? lazy.PlacesUIUtils.getImageURL(image)
    : "chrome://global/skin/icons/defaultFavicon.svg";
  let favicon = document.createElement("div");

  favicon.style.backgroundImage = `url('${imageUrl}')`;
  favicon.classList.add("favicon");
  return favicon;
}

export function onToggleContainer(detailsContainer) {
  // <details> elements fire `toggle` events when added to the DOM with the
  // "open" attribute set, but we don't want to record telemetry for those
  // as they aren't the result of user action. Ensure we bail out:
  if (
    detailsContainer.open &&
    detailsContainer.ownerDocument.readyState != "complete"
  ) {
    return;
  }

  const newFluentString = detailsContainer.open
    ? "firefoxview-collapse-button-hide"
    : "firefoxview-collapse-button-show";

  detailsContainer
    .querySelector(".twisty")
    .setAttribute("data-l10n-id", newFluentString);
  Services.telemetry.recordEvent(
    "firefoxview",
    detailsContainer.id === "tab-pickup-container"
      ? "tab_pickup_open"
      : "closed_tabs_open",
    "tabs",
    detailsContainer.open.toString()
  );
}
