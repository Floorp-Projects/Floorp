/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { actionCreators: ac, actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

const LINK_BLOCKED_EVENT = "newtab-linkBlocked";
const PLACES_LINKS_CHANGED_DELAY_TIME = 1000; // time in ms to delay timer for places links changed events

/**
 * Observer - a wrapper around history/bookmark observers to add the QueryInterface.
 */
class Observer {
  constructor(dispatch, observerInterface) {
    this.dispatch = dispatch;
    this.QueryInterface = ChromeUtils.generateQI([
      observerInterface,
      Ci.nsISupportsWeakReference,
    ]);
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
    this.dispatch({ type: at.PLACES_LINKS_CHANGED });
    this.dispatch({
      type: at.PLACES_LINK_DELETED,
      data: { url: uri.spec },
    });
  }

  /**
   * onClearHistory - Called when the user clears their entire history.
   */
  onClearHistory() {
    this.dispatch({ type: at.PLACES_HISTORY_CLEARED });
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
  // eslint-disable-next-line max-params
  onItemRemoved(id, folderId, index, type, uri, guid, parentGuid, source) {
    if (
      type === PlacesUtils.bookmarks.TYPE_BOOKMARK &&
      source !== PlacesUtils.bookmarks.SOURCES.IMPORT &&
      source !== PlacesUtils.bookmarks.SOURCES.RESTORE &&
      source !== PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP &&
      source !== PlacesUtils.bookmarks.SOURCES.SYNC
    ) {
      this.dispatch({ type: at.PLACES_LINKS_CHANGED });
      this.dispatch({
        type: at.PLACES_BOOKMARK_REMOVED,
        data: { url: uri.spec, bookmarkGuid: guid },
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

/**
 * PlacesObserver - observes events from PlacesUtils.observers
 */
class PlacesObserver extends Observer {
  constructor(dispatch) {
    super(dispatch, Ci.nsINavBookmarkObserver);
    this.handlePlacesEvent = this.handlePlacesEvent.bind(this);
  }

  handlePlacesEvent(events) {
    for (let {
      itemType,
      source,
      dateAdded,
      guid,
      title,
      url,
      isTagging,
    } of events) {
      // Skips items that are not bookmarks (like folders), about:* pages or
      // default bookmarks, added when the profile is created.
      if (
        isTagging ||
        itemType !== PlacesUtils.bookmarks.TYPE_BOOKMARK ||
        source === PlacesUtils.bookmarks.SOURCES.IMPORT ||
        source === PlacesUtils.bookmarks.SOURCES.RESTORE ||
        source === PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP ||
        source === PlacesUtils.bookmarks.SOURCES.SYNC ||
        (!url.startsWith("http://") && !url.startsWith("https://"))
      ) {
        return;
      }

      this.dispatch({ type: at.PLACES_LINKS_CHANGED });
      this.dispatch({
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          bookmarkGuid: guid,
          bookmarkTitle: title,
          dateAdded: dateAdded * 1000,
          url,
        },
      });
    }
  }
}

class PlacesFeed {
  constructor() {
    this.placesChangedTimer = null;
    this.customDispatch = this.customDispatch.bind(this);
    this.historyObserver = new HistoryObserver(this.customDispatch);
    this.bookmarksObserver = new BookmarksObserver(this.customDispatch);
    this.placesObserver = new PlacesObserver(this.customDispatch);
  }

  addObservers() {
    // NB: Directly get services without importing the *BIG* PlacesUtils module
    Cc["@mozilla.org/browser/nav-history-service;1"]
      .getService(Ci.nsINavHistoryService)
      .addObserver(this.historyObserver, true);
    Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
      .getService(Ci.nsINavBookmarksService)
      .addObserver(this.bookmarksObserver, true);
    PlacesUtils.observers.addListener(
      ["bookmark-added"],
      this.placesObserver.handlePlacesEvent
    );

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
    PlacesUtils.observers.removeListener(
      ["bookmark-added"],
      this.placesObserver.handlePlacesEvent
    );
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
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.PLACES_LINK_BLOCKED,
          data: { url: value },
        })
      );
    }
  }

  /**
   * Open a link in a desired destination defaulting to action's event.
   */
  openLink(action, where = "", isPrivate = false) {
    const params = {
      private: isPrivate,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    };

    // Always include the referrer (even for http links) if we have one
    const { event, referrer, typedBonus } = action.data;
    if (referrer) {
      const ReferrerInfo = Components.Constructor(
        "@mozilla.org/referrer-info;1",
        "nsIReferrerInfo",
        "init"
      );
      params.referrerInfo = new ReferrerInfo(
        Ci.nsIReferrerInfo.UNSAFE_URL,
        true,
        Services.io.newURI(referrer)
      );
    }

    // Pocket gives us a special reader URL to open their stories in
    const urlToOpen =
      action.data.type === "pocket" ? action.data.open_url : action.data.url;

    // Mark the page as typed for frecency bonus before opening the link
    if (typedBonus) {
      PlacesUtils.history.markPageAsTyped(Services.io.newURI(urlToOpen));
    }

    const win = action._target.browser.ownerGlobal;
    win.openLinkIn(urlToOpen, where || win.whereToOpenLink(event), params);
  }

  async saveToPocket(site, browser) {
    const { url, title } = site;
    try {
      let data = await NewTabUtils.activityStreamLinks.addPocketEntry(
        url,
        title,
        browser
      );
      if (data) {
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PLACES_SAVED_TO_POCKET,
            data: {
              url,
              open_url: data.item.open_url,
              title,
              pocket_id: data.item.item_id,
            },
          })
        );
      }
    } catch (err) {
      Cu.reportError(err);
    }
  }

  /**
   * Deletes an item from a user's saved to Pocket feed
   * @param {int} itemID
   *  The unique ID given by Pocket for that item; used to look the item up when deleting
   */
  async deleteFromPocket(itemID) {
    try {
      await NewTabUtils.activityStreamLinks.deletePocketEntry(itemID);
      this.store.dispatch({ type: at.POCKET_LINK_DELETED_OR_ARCHIVED });
    } catch (err) {
      Cu.reportError(err);
    }
  }

  /**
   * Archives an item from a user's saved to Pocket feed
   * @param {int} itemID
   *  The unique ID given by Pocket for that item; used to look the item up when archiving
   */
  async archiveFromPocket(itemID) {
    try {
      await NewTabUtils.activityStreamLinks.archivePocketEntry(itemID);
      this.store.dispatch({ type: at.POCKET_LINK_DELETED_OR_ARCHIVED });
    } catch (err) {
      Cu.reportError(err);
    }
  }

  fillSearchTopSiteTerm({ _target, data }) {
    _target.browser.ownerGlobal.gURLBar.search(`${data.label} `);
  }

  _getSearchPrefix() {
    const searchAliases =
      Services.search.defaultEngine.wrappedJSObject.__internalAliases;
    if (searchAliases && searchAliases.length > 0) {
      return `${searchAliases[0]} `;
    }
    return "";
  }

  handoffSearchToAwesomebar({ _target, data, meta }) {
    const searchAlias = this._getSearchPrefix();
    const urlBar = _target.browser.ownerGlobal.gURLBar;
    let isFirstChange = true;

    if (!data || !data.text) {
      urlBar.setHiddenFocus();
    } else {
      // Pass the provided text to the awesomebar. Prepend the @engine shortcut.
      urlBar.search(`${searchAlias}${data.text}`);
      isFirstChange = false;
    }

    const checkFirstChange = () => {
      // Check if this is the first change since we hidden focused. If it is,
      // remove hidden focus styles, prepend the search alias and hide the
      // in-content search.
      if (isFirstChange) {
        isFirstChange = false;
        urlBar.removeHiddenFocus();
        urlBar.search(searchAlias);
        this.store.dispatch(
          ac.OnlyToOneContent({ type: at.HIDE_SEARCH }, meta.fromTarget)
        );
        urlBar.removeEventListener("compositionstart", checkFirstChange);
        urlBar.removeEventListener("paste", checkFirstChange);
      }
    };

    const onKeydown = ev => {
      // Check if the keydown will cause a value change.
      if (ev.key.length === 1 && !ev.altKey && !ev.ctrlKey && !ev.metaKey) {
        checkFirstChange();
      }
      // If the Esc button is pressed, we are done. Show in-content search and cleanup.
      if (ev.key === "Escape") {
        onDone(); // eslint-disable-line no-use-before-define
      }
    };

    const onDone = () => {
      // We are done. Show in-content search again and cleanup.
      this.store.dispatch(
        ac.OnlyToOneContent({ type: at.SHOW_SEARCH }, meta.fromTarget)
      );
      urlBar.removeHiddenFocus();

      urlBar.removeEventListener("keydown", onKeydown);
      urlBar.removeEventListener("mousedown", onDone);
      urlBar.removeEventListener("blur", onDone);
      urlBar.removeEventListener("compositionstart", checkFirstChange);
      urlBar.removeEventListener("paste", checkFirstChange);
    };

    urlBar.addEventListener("keydown", onKeydown);
    urlBar.addEventListener("mousedown", onDone);
    urlBar.addEventListener("blur", onDone);
    urlBar.addEventListener("compositionstart", checkFirstChange);
    urlBar.addEventListener("paste", checkFirstChange);
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
        const { url, pocket_id } = action.data;
        NewTabUtils.activityStreamLinks.blockURL({ url, pocket_id });
        break;
      }
      case at.BOOKMARK_URL:
        NewTabUtils.activityStreamLinks.addBookmark(
          action.data,
          action._target.browser.ownerGlobal
        );
        break;
      case at.DELETE_BOOKMARK_BY_ID:
        NewTabUtils.activityStreamLinks.deleteBookmark(action.data);
        break;
      case at.DELETE_HISTORY_URL: {
        const { url, forceBlock, pocket_id } = action.data;
        NewTabUtils.activityStreamLinks.deleteHistoryEntry(url);
        if (forceBlock) {
          NewTabUtils.activityStreamLinks.blockURL({ url, pocket_id });
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
      case at.DELETE_FROM_POCKET:
        this.deleteFromPocket(action.data.pocket_id);
        break;
      case at.ARCHIVE_FROM_POCKET:
        this.archiveFromPocket(action.data.pocket_id);
        break;
      case at.FILL_SEARCH_TERM:
        this.fillSearchTopSiteTerm(action);
        break;
      case at.HANDOFF_SEARCH_TO_AWESOMEBAR:
        this.handoffSearchToAwesomebar(action);
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
PlacesFeed.PlacesObserver = PlacesObserver;

const EXPORTED_SYMBOLS = ["PlacesFeed"];
