/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const TOP_SITES_SOURCE = "TOP_SITES";
export const TOP_SITES_CONTEXT_MENU_OPTIONS = [
  "CheckPinTopSite",
  "EditTopSite",
  "Separator",
  "OpenInNewWindow",
  "OpenInPrivateWindow",
  "Separator",
  "BlockUrl",
  "DeleteUrl",
];
export const TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS = [
  "PinTopSite",
  "Separator",
  "OpenInNewWindow",
  "OpenInPrivateWindow",
  "Separator",
  "BlockUrl",
  "ShowPrivacyInfo",
];
export const TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS = [
  "OpenInNewWindow",
  "OpenInPrivateWindow",
  "Separator",
  "BlockUrl",
  "AboutSponsored",
];
// the special top site for search shortcut experiment can only have the option to unpin (which removes) the topsite
export const TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS = [
  "CheckPinTopSite",
  "Separator",
  "BlockUrl",
];
// minimum size necessary to show a rich icon instead of a screenshot
export const MIN_RICH_FAVICON_SIZE = 96;
// minimum size necessary to show any icon
export const MIN_SMALL_FAVICON_SIZE = 16;
