// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter, ContextMenuItemAlias } from "../types.ts";

const BROWSER_DOCUMENT_URI = "chrome://browser/content/browser.xhtml";
const PLACES_DOCUMENT_URI = "chrome://browser/content/places/places.xhtml";

function createFirefoxPopupAdapter(options: {
  key: string;
  label: string;
  documentURI: string;
  popupSelectors: readonly string[];
  aliases?: readonly ContextMenuItemAlias[];
  readonlySelectors?: readonly string[];
}): ContextMenuAdapter {
  return {
    key: options.key,
    label: options.label,
    documentURIs: [options.documentURI],
    popupSelectors: options.popupSelectors,
    aliases: options.aliases ?? [],
    readonlySelectors: options.readonlySelectors ?? ["[ext-type]"],
    profiles: [{ key: "default", label: "Default" }],
    getProfileKey: () => "default",
  };
}

/**
 * Firefox chrome popups that are not covered by the larger semantic adapters.
 *
 * Each adapter is intentionally scoped to one document and an allowlisted
 * popup. In particular, downloadsContextMenu exists in both browser.xhtml and
 * places.xhtml; distinct surface keys keep their catalogs and preferences from
 * being merged simply because Firefox reused the DOM id.
 */
export const firefoxPopupContextMenuAdapters: readonly ContextMenuAdapter[] = [
  createFirefoxPopupAdapter({
    key: "browser.navigation-history",
    label: "Back and forward history",
    documentURI: BROWSER_DOCUMENT_URI,
    popupSelectors: [
      '[id="backForwardMenu"]',
      '[id="back-button"] > menupopup:not([id])[context=""]',
      '[id="forward-button"] > menupopup:not([id])[context=""]',
    ],
  }),
  createFirefoxPopupAdapter({
    key: "browser.new-tab-button",
    label: "New tab button",
    documentURI: BROWSER_DOCUMENT_URI,
    popupSelectors: [
      '[id="new-tab-button-popup"]',
      '[id="new-tab-button"] > menupopup.new-tab-popup',
      '[id="tabs-newtab-button"] > menupopup.new-tab-popup',
      '[id="vertical-tabs-newtab-button"] > menupopup.new-tab-popup',
    ],
    readonlySelectors: ["[ext-type]", "[data-usercontextid]"],
  }),
  createFirefoxPopupAdapter({
    key: "browser.downloads",
    label: "Downloads panel",
    documentURI: BROWSER_DOCUMENT_URI,
    popupSelectors: ['[id="downloadsContextMenu"]'],
  }),
  createFirefoxPopupAdapter({
    key: "browser.split-view",
    label: "Split view",
    documentURI: BROWSER_DOCUMENT_URI,
    popupSelectors: ['[id="split-view-menu"]'],
    aliases: [
      {
        key: "split-view.separate-tabs",
        selectors: ['[command="splitViewCmd_separateTabs"]'],
      },
      {
        key: "split-view.reverse-tabs",
        selectors: ['[command="splitViewCmd_reverseTabs"]'],
      },
      {
        key: "split-view.separator",
        selectors: ['[id="split-view-menu"] > menuseparator'],
      },
      {
        key: "split-view.close-tabs",
        selectors: ['[command="splitViewCmd_closeTabs"]'],
      },
    ],
  }),
  createFirefoxPopupAdapter({
    key: "places.library-columns",
    label: "Library columns",
    documentURI: PLACES_DOCUMENT_URI,
    popupSelectors: ['[id="placesColumnsContext"]'],
  }),
  createFirefoxPopupAdapter({
    key: "places.library-downloads",
    label: "Library downloads",
    documentURI: PLACES_DOCUMENT_URI,
    popupSelectors: ['[id="downloadsContextMenu"]'],
  }),
];
