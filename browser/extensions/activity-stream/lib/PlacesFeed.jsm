/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const LINK_BLOCKED_EVENT = "newtab-linkBlocked";
const PLACES_LINKS_CHANGED_DELAY_TIME = 1000; // time in ms to delay timer for places links changed events

/**
 * Observer - a wrapper around history/bookmark observers to add the QueryInterface.
 */
class Observer {
  constructor(dispatch, observerInterface) {
    this.dispatch = dispatch;
    this.QueryInterface = ChromeUtils.generateQI([observerInterface, Ci.nsISupportsWeakReference]);
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
    this.dispatch({type: at.PLACES_LINKS_CHANGED});
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

  // Empty functions to make xpconnect happy
  onBeginUpdateBatch() {}

  onEndUpdateBatch() {}


  onTitleChanged() {}

  onFrecencyChanged() {}

  onManyFrecenciesChanged() {}

  onPageChanged() {}

  onDeleteVisits() {}
}

/**
 * BookmarksObserver - observes events from PlacesUtils.bookmarks
 */
class BookmarksObserver extends Observer {
  constructor(dispatch) {
    super(dispatch, Ci.nsINavBookmarkObserver);
    this.skipTags = true;
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
   * @param  {str} parent guid
   * @param  {int} source    Used to distinguish bookmarks made by different
   *                         actions: sync, bookmarks import, other.
   */
  onItemAdded(id, folderId, index, type, uri, bookmarkTitle, dateAdded, bookmarkGuid, parentGuid, source) { // eslint-disable-line max-params
    // Skips items that are not bookmarks (like folders), about:* pages or
    // default bookmarks, added when the profile is created.
    if (type !== PlacesUtils.bookmarks.TYPE_BOOKMARK ||
        source === PlacesUtils.bookmarks.SOURCES.IMPORT ||
        source === PlacesUtils.bookmarks.SOURCES.RESTORE ||
        source === PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP ||
        source === PlacesUtils.bookmarks.SOURCES.SYNC ||
        (uri.scheme !== "http" && uri.scheme !== "https")) {
      return;
    }

    this.dispatch({type: at.PLACES_LINKS_CHANGED});
    this.dispatch({
      type: at.PLACES_BOOKMARK_ADDED,
      data: {
        bookmarkGuid,
        bookmarkTitle,
        dateAdded,
        url: uri.spec
      }
    });
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
  onItemRemoved(id, folderId, index, type, uri, guid, parentGuid, source) { // eslint-disable-line max-params
    if (type === PlacesUtils.bookmarks.TYPE_BOOKMARK &&
        source !== PlacesUtils.bookmarks.SOURCES.IMPORT &&
        source !== PlacesUtils.bookmarks.SOURCES.RESTORE &&
        source !== PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP &&
        source !== PlacesUtils.bookmarks.SOURCES.SYNC) {
      this.dispatch({type: at.PLACES_LINKS_CHANGED});
      this.dispatch({
        type: at.PLACES_BOOKMARK_REMOVED,
        data: {url: uri.spec, bookmarkGuid: guid}
      });
    }
  }

  // Empty functions to make xpconnect happy
  onBeginUpdateBatch() {}

  onEndUpdateBatch() {}

  onItemVisited() {}

  onItemMoved() {}

  // Disabled due to performance cost, see Issue 3203 /
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1392267.
  onItemChanged() {}
}

class PlacesFeed {
  constructor() {
    this.placesChangedTimer = null;
    this.customDispatch = this.customDispatch.bind(this);
    this.historyObserver = new HistoryObserver(this.customDispatch);
    this.bookmarksObserver = new BookmarksObserver(this.customDispatch);
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

  /**
   * setTimeout - A custom function that creates an nsITimer that can be cancelled
   *
   * @param {func} callback       A function to be executed after the timer expires
   * @param {int}  delay          The time (in ms) the timer should wait before the function is executed
   */
  setTimeout(callback, delay) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(callback, delay, Ci.nsITimer.TYPE_ONE_SHOT);
    return timer;
  }

  customDispatch(action) {
    // If we are changing many links at once, delay this action and only dispatch
    // one action at the end
    if (action.type === at.PLACES_LINKS_CHANGED) {
      if (this.placesChangedTimer) {
        this.placesChangedTimer.delay = PLACES_LINKS_CHANGED_DELAY_TIME;
      } else {
        this.placesChangedTimer = this.setTimeout(() => {
          this.placesChangedTimer = null;
          this.store.dispatch(ac.OnlyToMain(action));
        }, PLACES_LINKS_CHANGED_DELAY_TIME);
      }
    } else {
      this.store.dispatch(ac.BroadcastToContent(action));
    }
  }

  removeObservers() {
    if (this.placesChangedTimer) {
      this.placesChangedTimer.cancel();
      this.placesChangedTimer = null;
    }
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

  /**
   * Open a link in a desired destination defaulting to action's event.
   */
  openLink(action, where = "", isPrivate = false) {
    const params = {
      private: isPrivate,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})
    };

    // Always include the referrer (even for http links) if we have one
    const {event, referrer, typedBonus} = action.data;
    if (referrer) {
      params.referrerPolicy = Ci.nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL;
      params.referrerURI = Services.io.newURI(referrer);
    }

    // Pocket gives us a special reader URL to open their stories in
    const urlToOpen = action.data.type === "pocket" ? action.data.open_url : action.data.url;

    // Mark the page as typed for frecency bonus before opening the link
    if (typedBonus) {
      PlacesUtils.history.markPageAsTyped(Services.io.newURI(urlToOpen));
    }

    const win = action._target.browser.ownerGlobal;
    win.openLinkIn(urlToOpen, where || win.whereToOpenLink(event), params);
  }

  async saveToPocket(site, browser) {
    const {url, title} = site;
    try {
      let data = await NewTabUtils.activityStreamLinks.addPocketEntry(url, title, browser);
      if (data) {
        this.store.dispatch(ac.BroadcastToContent({
          type: at.PLACES_SAVED_TO_POCKET,
          data: {url, open_url: data.item.open_url, title, pocket_id: data.item.item_id}
        }));
      }
    } catch (err) {
      Cu.reportError(err);
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        // Briefly avoid loading services for observing for better startup timing
        Services.tm.dispatchToMainThread(() => this.addObservers());
        break;
      case at.UNINIT:
        this.removeObservers();
        break;
      case at.BLOCK_URL: {
        const {url, pocket_id} = action.data;
        NewTabUtils.activityStreamLinks.blockURL({url, pocket_id});
        break;
      }
      case at.BOOKMARK_URL:
        NewTabUtils.activityStreamLinks.addBookmark(action.data, action._target.browser);
        break;
      case at.DELETE_BOOKMARK_BY_ID:
        NewTabUtils.activityStreamLinks.deleteBookmark(action.data);
        break;
      case at.DELETE_HISTORY_URL: {
        const {url, forceBlock, pocket_id} = action.data;
        NewTabUtils.activityStreamLinks.deleteHistoryEntry(url);
        if (forceBlock) {
          NewTabUtils.activityStreamLinks.blockURL({url, pocket_id});
        }
        break;
      }
      case at.OPEN_NEW_WINDOW:
        this.openLink(action, "window");
        break;
      case at.OPEN_PRIVATE_WINDOW:
        this.openLink(action, "window", true);
        break;
      case at.SAVE_TO_POCKET:
        this.saveToPocket(action.data.site, action._target.browser);
        break;
      case at.OPEN_LINK: {
        this.openLink(action);
        break;
      }
    }
  }
}

this.PlacesFeed = PlacesFeed;

// Exported for testing only
PlacesFeed.HistoryObserver = HistoryObserver;
PlacesFeed.BookmarksObserver = BookmarksObserver;

const EXPORTED_SYMBOLS = ["PlacesFeed"];
