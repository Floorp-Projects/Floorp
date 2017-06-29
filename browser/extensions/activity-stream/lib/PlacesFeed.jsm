/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const LINK_BLOCKED_EVENT = "newtab-linkBlocked";

/**
 * Observer - a wrapper around history/bookmark observers to add the QueryInterface.
 */
class Observer {
  constructor(dispatch, observerInterface) {
    this.dispatch = dispatch;
    this.QueryInterface = XPCOMUtils.generateQI([observerInterface, Ci.nsISupportsWeakReference]);
  }
}

/**
 * HistoryObserver - observes events from PlacesUtils.history
 */
class HistoryObserver extends Observer {
  constructor(dispatch) {
    super(dispatch, Ci.nsINavHistoryObserver);
  }

  /**
   * onDeleteURI - Called when an link is deleted from history.
   *
   * @param  {obj} uri        A URI object representing the link's url
   *         {str} uri.spec   The URI as a string
   */
  onDeleteURI(uri) {
    this.dispatch({
      type: at.PLACES_LINK_DELETED,
      data: {url: uri.spec}
    });
  }

  /**
   * onClearHistory - Called when the user clears their entire history.
   */
  onClearHistory() {
    this.dispatch({type: at.PLACES_HISTORY_CLEARED});
  }
}

/**
 * BookmarksObserver - observes events from PlacesUtils.bookmarks
 */
class BookmarksObserver extends Observer {
  constructor(dispatch) {
    super(dispatch, Ci.nsINavBookmarkObserver);
  }

  /**
   * onItemAdded - Called when a bookmark is added
   *
   * @param  {str} id
   * @param  {str} folderId
   * @param  {int} index
   * @param  {int} type       Indicates if the bookmark is an actual bookmark,
   *                          a folder, or a separator.
   * @param  {str} uri
   * @param  {str} title
   * @param  {int} dateAdded
   * @param  {str} guid      The unique id of the bookmark
   */
  async onItemAdded(...args) {
    const type = args[3];
    const guid = args[7];
    if (type !== PlacesUtils.bookmarks.TYPE_BOOKMARK) {
      return;
    }
    try {
      // bookmark: {bookmarkGuid, bookmarkTitle, lastModified, url}
      const bookmark = await NewTabUtils.activityStreamProvider.getBookmark(guid);
      this.dispatch({type: at.PLACES_BOOKMARK_ADDED, data: bookmark});
    } catch (e) {
      Cu.reportError(e);
    }
  }

  /**
   * onItemRemoved - Called when a bookmark is removed
   *
   * @param  {str} id
   * @param  {str} folderId
   * @param  {int} index
   * @param  {int} type       Indicates if the bookmark is an actual bookmark,
   *                          a folder, or a separator.
   * @param  {str} uri
   * @param  {str} guid      The unique id of the bookmark
   */
  onItemRemoved(id, folderId, index, type, uri, guid) {
    if (type === PlacesUtils.bookmarks.TYPE_BOOKMARK) {
      this.dispatch({
        type: at.PLACES_BOOKMARK_REMOVED,
        data: {url: uri.spec, bookmarkGuid: guid}
      });
    }
  }

  /**
   * onItemChanged - Called when a bookmark is modified
   *
   * @param  {str} id           description
   * @param  {str} property     The property that was modified (e.g. uri, title)
   * @param  {bool} isAnnotation
   * @param  {any} value
   * @param  {int} lastModified
   * @param  {int} type         Indicates if the bookmark is an actual bookmark,
   *                             a folder, or a separator.
   * @param  {int} parent
   * @param  {str} guid         The unique id of the bookmark
   */
  async onItemChanged(...args) {
    const property = args[1];
    const type = args[5];
    const guid = args[7];

    // Only process this event if it is a TYPE_BOOKMARK, and uri or title was the property changed.
    if (type !== PlacesUtils.bookmarks.TYPE_BOOKMARK || !["uri", "title"].includes(property)) {
      return;
    }
    try {
      // bookmark: {bookmarkGuid, bookmarkTitle, lastModified, url}
      const bookmark = await NewTabUtils.activityStreamProvider.getBookmark(guid);
      this.dispatch({type: at.PLACES_BOOKMARK_CHANGED, data: bookmark});
    } catch (e) {
      Cu.reportError(e);
    }
  }
}

class PlacesFeed {
  constructor() {
    this.historyObserver = new HistoryObserver(action => this.store.dispatch(ac.BroadcastToContent(action)));
    this.bookmarksObserver = new BookmarksObserver(action => this.store.dispatch(ac.BroadcastToContent(action)));
  }

  addObservers() {
    // NB: Directly get services without importing the *BIG* PlacesUtils module
    Cc["@mozilla.org/browser/nav-history-service;1"]
      .getService(Ci.nsINavHistoryService)
      .addObserver(this.historyObserver, true);
    Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
      .getService(Ci.nsINavBookmarksService)
      .addObserver(this.bookmarksObserver, true);

    Services.obs.addObserver(this, LINK_BLOCKED_EVENT);
  }

  removeObservers() {
    PlacesUtils.history.removeObserver(this.historyObserver);
    PlacesUtils.bookmarks.removeObserver(this.bookmarksObserver);
    Services.obs.removeObserver(this, LINK_BLOCKED_EVENT);
  }

  /**
   * observe - An observer for the LINK_BLOCKED_EVENT.
   *           Called when a link is blocked.
   *
   * @param  {null} subject
   * @param  {str} topic   The name of the event
   * @param  {str} value   The data associated with the event
   */
  observe(subject, topic, value) {
    if (topic === LINK_BLOCKED_EVENT) {
      this.store.dispatch(ac.BroadcastToContent({
        type: at.PLACES_LINK_BLOCKED,
        data: {url: value}
      }));
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.addObservers();
        break;
      case at.UNINIT:
        this.removeObservers();
        break;
      case at.BLOCK_URL:
        NewTabUtils.activityStreamLinks.blockURL({url: action.data});
        break;
      case at.BOOKMARK_URL:
        NewTabUtils.activityStreamLinks.addBookmark(action.data);
        break;
      case at.DELETE_BOOKMARK_BY_ID:
        NewTabUtils.activityStreamLinks.deleteBookmark(action.data);
        break;
      case at.DELETE_HISTORY_URL:
        NewTabUtils.activityStreamLinks.deleteHistoryEntry(action.data);
        break;
    }
  }
}

this.PlacesFeed = PlacesFeed;

// Exported for testing only
PlacesFeed.HistoryObserver = HistoryObserver;
PlacesFeed.BookmarksObserver = BookmarksObserver;

this.EXPORTED_SYMBOLS = ["PlacesFeed"];
