// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter } from "../types.ts";

function getPlacesProfileKey(window: Window): string {
  const uri = window.document.documentURI.split(/[?#]/, 1)[0];
  switch (uri) {
    case "chrome://browser/content/places/places.xhtml":
      return "library";
    case "chrome://browser/content/places/bookmarksSidebar.xhtml":
      return "bookmarks-sidebar";
    case "chrome://browser/content/places/historySidebar.xhtml":
      return "history-sidebar";
    default:
      return "browser";
  }
}

export const placesContextMenuAdapter: ContextMenuAdapter = {
  key: "browser.places",
  label: "Bookmarks and history",
  documentURIs: [
    "chrome://browser/content/browser.xhtml",
    "chrome://browser/content/places/places.xhtml",
    "chrome://browser/content/places/bookmarksSidebar.xhtml",
    "chrome://browser/content/places/historySidebar.xhtml",
  ],
  popupSelectors: ["#placesContext"],
  aliases: [
    { key: "places.open", selectors: ['[id="placesContext_open"]'] },
    {
      key: "places.open-new-tab",
      selectors: ['[id="placesContext_open:newtab"]'],
    },
    {
      key: "places.open-new-window",
      selectors: ['[id="placesContext_open:newwindow"]'],
    },
    {
      key: "places.open-private-window",
      selectors: ['[id="placesContext_open:newprivatewindow"]'],
    },
    { key: "places.show-info", selectors: ['[id="placesContext_show:info"]'] },
    { key: "places.delete", selectors: ['[id="placesContext_delete"]'] },
    { key: "places.cut", selectors: ['[id="placesContext_cut"]'] },
    { key: "places.copy", selectors: ['[id="placesContext_copy"]'] },
    { key: "places.paste", selectors: ['[id="placesContext_paste"]'] },
    {
      key: "places.new-bookmark",
      selectors: ['[id="placesContext_new:bookmark"]'],
    },
    {
      key: "places.new-folder",
      selectors: ['[id="placesContext_new:folder"]'],
    },
  ],
  readonlySelectors: ["[ext-type]"],
  profiles: [
    { key: "browser", label: "Browser window" },
    { key: "library", label: "Library" },
    { key: "bookmarks-sidebar", label: "Bookmarks sidebar" },
    { key: "history-sidebar", label: "History sidebar" },
  ],
  getProfileKey: getPlacesProfileKey,
};
