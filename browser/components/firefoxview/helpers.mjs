/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(globalThis, {
  Services: "resource://gre/modules/Services.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
});

export function formatURIForDisplay(uriString) {
  // TODO: Bug 1764816: Make sure we handle file:///, jar:, blob, IP4/IP6 etc. addresses
  let uri;
  try {
    uri = Services.io.newURI(uriString);
  } catch (ex) {
    return uriString;
  }
  if (!uri.asciiHost) {
    return uriString;
  }
  let displayHost;
  try {
    // This might fail if it's an IP address or doesn't have more than 1 part
    displayHost = Services.eTLD.getBaseDomain(uri);
  } catch (ex) {
    return uri.displayHostPort;
  }
  return displayHost.length ? displayHost : uriString;
}

export function convertTimestamp(timestamp, fluentStrings) {
  const relativeTimeFormat = new Services.intl.RelativeTimeFormat(
    undefined,
    {}
  );
  const elapsed = Date.now() - timestamp;
  // Cutoff of 1.5 minutes + 1 second to determine what text string to display
  const nowThresholdMs = 91000;
  let formattedTime;
  if (elapsed <= nowThresholdMs) {
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
    ? PlacesUIUtils.getImageURL(image)
    : "chrome://global/skin/icons/defaultFavicon.svg";
  let favicon = document.createElement("div");

  favicon.style.backgroundImage = `url('${imageUrl}')`;
  favicon.classList.add("favicon");
  return favicon;
}
