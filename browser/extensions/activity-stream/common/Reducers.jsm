/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionTypes: at} = Components.utils.import("resource://activity-stream/common/Actions.jsm", {});

const TOP_SITES_DEFAULT_LENGTH = 6;
const TOP_SITES_SHOWMORE_LENGTH = 12;

const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    // The locale of the browser
    locale: "",
    // Localized strings with defaults
    strings: null,
    // The version of the system-addon
    version: null
  },
  Snippets: {initialized: false},
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: []
  },
  Prefs: {
    initialized: false,
    values: {}
  },
  Dialog: {
    visible: false,
    data: {}
  },
  Sections: []
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case at.INIT:
      return Object.assign({}, prevState, action.data || {}, {initialized: true});
    case at.LOCALE_UPDATED: {
      if (!action.data) {
        return prevState;
      }
      let {locale, strings} = action.data;
      return Object.assign({}, prevState, {
        locale,
        strings
      });
    }
    default:
      return prevState;
  }
}

/**
 * insertPinned - Inserts pinned links in their specified slots
 *
 * @param {array} a list of links
 * @param {array} a list of pinned links
 * @return {array} resulting list of links with pinned links inserted
 */
function insertPinned(links, pinned) {
  // Remove any pinned links
  const pinnedUrls = pinned.map(link => link && link.url);
  let newLinks = links.filter(link => (link ? !pinnedUrls.includes(link.url) : false));
  newLinks = newLinks.map(link => {
    if (link && link.isPinned) {
      delete link.isPinned;
      delete link.pinIndex;
    }
    return link;
  });

  // Then insert them in their specified location
  pinned.forEach((val, index) => {
    if (!val) { return; }
    let link = Object.assign({}, val, {isPinned: true, pinIndex: index});
    if (index > newLinks.length) {
      newLinks[index] = link;
    } else {
      newLinks.splice(index, 0, link);
    }
  });

  return newLinks;
}

function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  let hasMatch;
  let newRows;
  let pinned;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, {initialized: true, rows: action.data});
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, {screenshot: action.data.screenshot});
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, {rows: newRows}) : prevState;
    case at.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const {bookmarkGuid, bookmarkTitle, lastModified} = action.data;
          return Object.assign({}, site, {bookmarkGuid, bookmarkTitle, bookmarkDateCreated: lastModified});
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const newSite = Object.assign({}, site);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;
          return newSite;
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.BLOCK_URL:
    case at.DELETE_HISTORY_URL:
      // Optimistically update the UI by responding to the context menu action
      // events and removing the site that was blocked/deleted with an empty slot.
      // Once refresh() finishes, we update the UI again with a new site
      newRows = prevState.rows.filter(val => val && val.url !== action.data.url);
      return Object.assign({}, prevState, {rows: newRows});
    case at.PINNED_SITES_UPDATED:
      pinned = action.data;
      newRows = insertPinned(prevState.rows, pinned).slice(0, TOP_SITES_SHOWMORE_LENGTH);
      return Object.assign({}, prevState, {rows: newRows});
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case at.DIALOG_OPEN:
      return Object.assign({}, prevState, {visible: true, data: action.data});
    case at.DIALOG_CANCEL:
      return Object.assign({}, prevState, {visible: false});
    case at.DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case at.PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, {initialized: true, values: action.data});
    case at.PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, {values: newValues});
    default:
      return prevState;
  }
}

function Sections(prevState = INITIAL_STATE.Sections, action) {
  let hasMatch;
  let newState;
  switch (action.type) {
    case at.SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case at.SECTION_REGISTER:
      // If section exists in prevState, update it
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          hasMatch = true;
          return Object.assign({}, section, action.data);
        }
        return section;
      });

      // Invariant: Sections array sorted in increasing order of property `order`.
      // If section doesn't exist in prevState, create a new section object. If
      // the section has an order, insert it at the correct place in the array.
      // Otherwise, prepend it and set the order to be minimal.
      if (!hasMatch) {
        const initialized = action.data.rows && action.data.rows.length > 0;
        let order;
        let index;
        if (prevState.length > 0) {
          order = action.data.order || prevState[0].order - 1;
          index = newState.findIndex(section => section.order >= order);
        } else {
          order = action.data.order || 1;
          index = 0;
        }
        const section = Object.assign({title: "", initialized, rows: [], order, enabled: false}, action.data);
        newState.splice(index, 0, section);
      }
      return newState;
    case at.SECTION_UPDATE:
      return prevState.map(section => {
        if (section && section.id === action.data.id) {
          return Object.assign({}, section, action.data);
        }
        return section;
      });
    case at.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the item within the rows that is attempted to be bookmarked
          if (item.url === action.data.url) {
            const {bookmarkGuid, bookmarkTitle, lastModified} = action.data;
            Object.assign(item, {bookmarkGuid, bookmarkTitle, bookmarkDateCreated: lastModified});
          }
          return item;
        })
      }));
    case at.PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the bookmark within the rows that is attempted to be removed
          if (item.url === action.data.url) {
            const newSite = Object.assign({}, item);
            delete newSite.bookmarkGuid;
            delete newSite.bookmarkTitle;
            delete newSite.bookmarkDateCreated;
            return newSite;
          }
          return item;
        })
      }));
    case at.PLACES_LINK_DELETED:
    case at.PLACES_LINK_BLOCKED:
      return prevState.map(section =>
        Object.assign({}, section, {rows: section.rows.filter(site => site.url !== action.data.url)}));
    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case at.SNIPPETS_DATA:
      return Object.assign({}, prevState, {initialized: true}, action.data);
    case at.SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

this.INITIAL_STATE = INITIAL_STATE;
this.TOP_SITES_DEFAULT_LENGTH = TOP_SITES_DEFAULT_LENGTH;
this.TOP_SITES_SHOWMORE_LENGTH = TOP_SITES_SHOWMORE_LENGTH;

this.reducers = {TopSites, App, Snippets, Prefs, Dialog, Sections};
this.insertPinned = insertPinned;

this.EXPORTED_SYMBOLS = ["reducers", "INITIAL_STATE", "insertPinned", "TOP_SITES_DEFAULT_LENGTH", "TOP_SITES_SHOWMORE_LENGTH"];
