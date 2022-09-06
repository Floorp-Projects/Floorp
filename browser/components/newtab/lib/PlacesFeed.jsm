/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const {
  actionCreators: ac,
  actionTypes: at,
  actionUtils: au,
} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm");
const { shortURL } = ChromeUtils.import(
  "resource://activity-stream/lib/ShortURL.jsm"
);
const { AboutNewTab } = ChromeUtils.import(
  "resource:///modules/AboutNewTab.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PartnerLinkAttribution",
  "resource:///modules/PartnerLinkAttribution.jsm"
);
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "pktApi",
  "chrome://pocket/content/pktApi.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ExperimentAPI",
  "resource://nimbus/ExperimentAPI.jsm"
);
XPCOMUtils.defineLazyModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

const LINK_BLOCKED_EVENT = "newtab-linkBlocked";
const PLACES_LINKS_CHANGED_DELAY_TIME = 1000; // time in ms to delay timer for places links changed events

// The pref to store the blocked sponsors of the sponsored Top Sites.
// The value of this pref is an array (JSON serialized) of hostnames of the
// blocked sponsors.
const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";

/**
 * Observer - a wrapper around history/bookmark observers to add the QueryInterface.
 */
class Observer {
  constructor(dispatch, observerInterface) {
    this.dispatch = dispatch;
    this.QueryInterface = ChromeUtils.generateQI([
      observerInterface,
      "nsISupportsWeakReference",
    ]);
  }
}

/**
 * BookmarksObserver - observes events from PlacesUtils.bookmarks
 */
class BookmarksObserver extends Observer {
  constructor(dispatch) {
    super(dispatch, Ci.nsINavBookmarkObserver);
    this.skipTags = true;
  }

  // Empty functions to make xpconnect happy.
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
    const removedPages = [];
    const removedBookmarks = [];

    for (const {
      itemType,
      source,
      dateAdded,
      guid,
      title,
      url,
      isRemovedFromStore,
      isTagging,
      type,
    } of events) {
      switch (type) {
        case "history-cleared":
          this.dispatch({ type: at.PLACES_HISTORY_CLEARED });
          break;
        case "page-removed":
          if (isRemovedFromStore) {
            removedPages.push(url);
          }
          break;
        case "bookmark-added":
          // Skips items that are not bookmarks (like folders), about:* pages or
          // default bookmarks, added when the profile is created.
          if (
            isTagging ||
            itemType !== lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK ||
            source === lazy.PlacesUtils.bookmarks.SOURCES.IMPORT ||
            source === lazy.PlacesUtils.bookmarks.SOURCES.RESTORE ||
            source === lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP ||
            source === lazy.PlacesUtils.bookmarks.SOURCES.SYNC ||
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
          break;
        case "bookmark-removed":
          if (
            isTagging ||
            (itemType === lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK &&
              source !== lazy.PlacesUtils.bookmarks.SOURCES.IMPORT &&
              source !== lazy.PlacesUtils.bookmarks.SOURCES.RESTORE &&
              source !==
                lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP &&
              source !== lazy.PlacesUtils.bookmarks.SOURCES.SYNC)
          ) {
            removedBookmarks.push(url);
          }
          break;
      }
    }

    if (removedPages.length || removedBookmarks.length) {
      this.dispatch({ type: at.PLACES_LINKS_CHANGED });
    }

    if (removedPages.length) {
      this.dispatch({
        type: at.PLACES_LINKS_DELETED,
        data: { urls: removedPages },
      });
    }

    if (removedBookmarks.length) {
      this.dispatch({
        type: at.PLACES_BOOKMARKS_REMOVED,
        data: { urls: removedBookmarks },
      });
    }
  }
}

class PlacesFeed {
  constructor() {
    this.placesChangedTimer = null;
    this.customDispatch = this.customDispatch.bind(this);
    this.bookmarksObserver = new BookmarksObserver(this.customDispatch);
    this.placesObserver = new PlacesObserver(this.customDispatch);
  }

  addObservers() {
    // NB: Directly get services without importing the *BIG* PlacesUtils module
    Cc["@mozilla.org/browser/nav-bookmarks-service;1"]
      .getService(Ci.nsINavBookmarksService)
      .addObserver(this.bookmarksObserver, true);
    lazy.PlacesUtils.observers.addListener(
      ["bookmark-added", "bookmark-removed", "history-cleared", "page-removed"],
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
    lazy.PlacesUtils.bookmarks.removeObserver(this.bookmarksObserver);
    lazy.PlacesUtils.observers.removeListener(
      ["bookmark-added", "bookmark-removed", "history-cleared", "page-removed"],
      this.placesObserver.handlePlacesEvent
    );
    Services.obs.removeObserver(this, LINK_BLOCKED_EVENT);
  }

  /**
   * observe - An observer for the LINK_BLOCKED_EVENT.
   *           Called when a link is blocked.
   *           Links can be blocked outside of newtab,
   *           which is why we need to listen to this
   *           on such a generic level.
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
      targetBrowser: action._target.browser,
      fromChrome: false, // This ensure we maintain user preference for how to open new tabs.
      globalHistoryOptions: {
        triggeringSponsoredURL: action.data.sponsored_tile_id
          ? action.data.url
          : undefined,
      },
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

    try {
      let uri = Services.io.newURI(urlToOpen);
      if (!["http", "https"].includes(uri.scheme)) {
        throw new Error(
          `Can't open link using ${uri.scheme} protocol from the new tab page.`
        );
      }
    } catch (e) {
      Cu.reportError(e);
      return;
    }

    // Mark the page as typed for frecency bonus before opening the link
    if (typedBonus) {
      lazy.PlacesUtils.history.markPageAsTyped(Services.io.newURI(urlToOpen));
    }

    const win = action._target.browser.ownerGlobal;
    win.openTrustedLinkIn(
      urlToOpen,
      where || win.whereToOpenLink(event),
      params
    );

    // If there's an original URL e.g. using the unprocessed %YYYYMMDDHH% tag,
    // add a visit for that so it may become a frecent top site.
    if (action.data.original_url) {
      lazy.PlacesUtils.history.insert({
        url: action.data.original_url,
        visits: [{ transition: lazy.PlacesUtils.history.TRANSITION_TYPED }],
      });
    }
  }

  async saveToPocket(site, browser) {
    const sendToPocket = lazy.NimbusFeatures.pocketNewtab.getVariable(
      "sendToPocket"
    );
    // An experiment to send the user directly to Pocket's signup page.
    if (sendToPocket && !lazy.pktApi.isUserLoggedIn()) {
      const pocketNewtabExperiment = lazy.ExperimentAPI.getExperiment({
        featureId: "pocketNewtab",
      });
      const pocketSiteHost = Services.prefs.getStringPref(
        "extensions.pocket.site"
      ); // getpocket.com
      let utmSource = "firefox_newtab_save_button";
      // We want to know if the user is in a Pocket newtab related experiment.
      let utmCampaign = pocketNewtabExperiment?.slug;
      let utmContent = pocketNewtabExperiment?.branch?.slug;

      const url = new URL(`https://${pocketSiteHost}/signup`);
      url.searchParams.append("utm_source", utmSource);
      if (utmCampaign && utmContent) {
        url.searchParams.append("utm_campaign", utmCampaign);
        url.searchParams.append("utm_content", utmContent);
      }

      const win = browser.ownerGlobal;
      win.openTrustedLinkIn(url.href, "tab");
      return;
    }

    const { url, title } = site;
    try {
      let data = await lazy.NewTabUtils.activityStreamLinks.addPocketEntry(
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
      await lazy.NewTabUtils.activityStreamLinks.deletePocketEntry(itemID);
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
      await lazy.NewTabUtils.activityStreamLinks.archivePocketEntry(itemID);
      this.store.dispatch({ type: at.POCKET_LINK_DELETED_OR_ARCHIVED });
    } catch (err) {
      Cu.reportError(err);
    }
  }

  /**
   * Sends an attribution request for Top Sites interactions.
   * @param {object} data
   *   Attribution paramters from a Top Site.
   */
  makeAttributionRequest(data) {
    let args = Object.assign(
      {
        campaignID: Services.prefs.getStringPref(
          "browser.partnerlink.campaign.topsites"
        ),
      },
      data
    );
    lazy.PartnerLinkAttribution.makeRequest(args);
  }

  async fillSearchTopSiteTerm({ _target, data }) {
    const searchEngine = await Services.search.getEngineByAlias(data.label);
    _target.browser.ownerGlobal.gURLBar.search(data.label, {
      searchEngine,
      searchModeEntry: "topsites_newtab",
    });
  }

  _getDefaultSearchEngine(isPrivateWindow) {
    return Services.search[
      isPrivateWindow ? "defaultPrivateEngine" : "defaultEngine"
    ];
  }

  handoffSearchToAwesomebar(action) {
    const { _target, data, meta } = action;
    const searchEngine = this._getDefaultSearchEngine(
      lazy.PrivateBrowsingUtils.isBrowserPrivate(_target.browser)
    );
    const urlBar = _target.browser.ownerGlobal.gURLBar;
    let isFirstChange = true;

    const newtabSession = AboutNewTab.activityStream.store.feeds
      .get("feeds.telemetry")
      ?.sessions.get(au.getPortIdOfSender(action));
    if (!data || !data.text) {
      urlBar.setHiddenFocus();
    } else {
      urlBar.handoff(data.text, searchEngine, newtabSession?.session_id);
      isFirstChange = false;
    }

    const checkFirstChange = () => {
      // Check if this is the first change since we hidden focused. If it is,
      // remove hidden focus styles, prepend the search alias and hide the
      // in-content search.
      if (isFirstChange) {
        isFirstChange = false;
        urlBar.removeHiddenFocus(true);
        urlBar.handoff("", searchEngine, newtabSession?.session_id);
        this.store.dispatch(
          ac.OnlyToOneContent({ type: at.DISABLE_SEARCH }, meta.fromTarget)
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

    const onDone = ev => {
      // We are done. Show in-content search again and cleanup.
      this.store.dispatch(
        ac.OnlyToOneContent({ type: at.SHOW_SEARCH }, meta.fromTarget)
      );

      const forceSuppressFocusBorder = ev?.type === "mousedown";
      urlBar.removeHiddenFocus(forceSuppressFocusBorder);

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

  /**
   * Add the hostnames of the given urls to the Top Sites sponsor blocklist.
   *
   * @param {array} urls
   *   An array of the objects structured as `{ url }`
   */
  addToBlockedTopSitesSponsors(urls) {
    const blockedPref = JSON.parse(
      Services.prefs.getStringPref(TOP_SITES_BLOCKED_SPONSORS_PREF, "[]")
    );
    const merged = new Set([...blockedPref, ...urls.map(url => shortURL(url))]);

    Services.prefs.setStringPref(
      TOP_SITES_BLOCKED_SPONSORS_PREF,
      JSON.stringify([...merged])
    );
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
      case at.ABOUT_SPONSORED_TOP_SITES: {
        const url = `${Services.urlFormatter.formatURLPref(
          "app.support.baseURL"
        )}sponsor-privacy`;
        const win = action._target.browser.ownerGlobal;
        win.openTrustedLinkIn(url, "tab");
        break;
      }
      case at.BLOCK_URL: {
        if (action.data) {
          let sponsoredTopSites = [];
          action.data.forEach(site => {
            const { url, pocket_id, isSponsoredTopSite } = site;
            lazy.NewTabUtils.activityStreamLinks.blockURL({ url, pocket_id });
            if (isSponsoredTopSite) {
              sponsoredTopSites.push({ url });
            }
          });
          if (sponsoredTopSites.length) {
            this.addToBlockedTopSitesSponsors(sponsoredTopSites);
          }
        }
        break;
      }
      case at.BOOKMARK_URL:
        lazy.NewTabUtils.activityStreamLinks.addBookmark(
          action.data,
          action._target.browser.ownerGlobal
        );
        break;
      case at.DELETE_BOOKMARK_BY_ID:
        lazy.NewTabUtils.activityStreamLinks.deleteBookmark(action.data);
        break;
      case at.DELETE_HISTORY_URL: {
        const { url, forceBlock, pocket_id } = action.data;
        lazy.NewTabUtils.activityStreamLinks.deleteHistoryEntry(url);
        if (forceBlock) {
          lazy.NewTabUtils.activityStreamLinks.blockURL({ url, pocket_id });
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
      case at.PARTNER_LINK_ATTRIBUTION:
        this.makeAttributionRequest(action.data);
        break;
    }
  }
}

// Exported for testing only
PlacesFeed.BookmarksObserver = BookmarksObserver;
PlacesFeed.PlacesObserver = PlacesObserver;

const EXPORTED_SYMBOLS = ["PlacesFeed"];
