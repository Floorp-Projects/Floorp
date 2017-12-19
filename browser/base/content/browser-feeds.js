/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_MAYBE_AUDIO_FEED = "application/vnd.mozilla.maybe.audio.feed";
const TYPE_MAYBE_VIDEO_FEED = "application/vnd.mozilla.maybe.video.feed";

const PREF_SHOW_FIRST_RUN_UI = "browser.feeds.showFirstRunUI";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_ACTION = "browser.feeds.handler";
const PREF_SELECTED_READER = "browser.feeds.handler.default";

const PREF_VIDEO_SELECTED_APP = "browser.videoFeeds.handlers.application";
const PREF_VIDEO_SELECTED_WEB = "browser.videoFeeds.handlers.webservice";
const PREF_VIDEO_SELECTED_ACTION = "browser.videoFeeds.handler";
const PREF_VIDEO_SELECTED_READER = "browser.videoFeeds.handler.default";

const PREF_AUDIO_SELECTED_APP = "browser.audioFeeds.handlers.application";
const PREF_AUDIO_SELECTED_WEB = "browser.audioFeeds.handlers.webservice";
const PREF_AUDIO_SELECTED_ACTION = "browser.audioFeeds.handler";
const PREF_AUDIO_SELECTED_READER = "browser.audioFeeds.handler.default";

const PREF_UPDATE_DELAY = 2000;

const SETTABLE_PREFS = new Set([
  PREF_VIDEO_SELECTED_ACTION,
  PREF_AUDIO_SELECTED_ACTION,
  PREF_SELECTED_ACTION,
  PREF_VIDEO_SELECTED_READER,
  PREF_AUDIO_SELECTED_READER,
  PREF_SELECTED_READER,
  PREF_VIDEO_SELECTED_WEB,
  PREF_AUDIO_SELECTED_WEB,
  PREF_SELECTED_WEB
]);

const EXECUTABLE_PREFS = new Set([
  PREF_SELECTED_APP,
  PREF_VIDEO_SELECTED_APP,
  PREF_AUDIO_SELECTED_APP
]);

const VALID_ACTIONS = new Set(["ask", "reader", "bookmarks"]);
const VALID_READERS = new Set(["web", "client", "default", "bookmarks"]);

XPCOMUtils.defineLazyPreferenceGetter(this, "SHOULD_LOG",
                                      "feeds.log", false);

function LOG(str) {
  if (SHOULD_LOG)
    dump("*** Feeds: " + str + "\n");
}

function getPrefActionForType(t) {
  switch (t) {
    case Ci.nsIFeed.TYPE_VIDEO:
      return PREF_VIDEO_SELECTED_ACTION;

    case Ci.nsIFeed.TYPE_AUDIO:
      return PREF_AUDIO_SELECTED_ACTION;

    default:
      return PREF_SELECTED_ACTION;
  }
}

function getPrefReaderForType(t) {
  switch (t) {
    case Ci.nsIFeed.TYPE_VIDEO:
      return PREF_VIDEO_SELECTED_READER;

    case Ci.nsIFeed.TYPE_AUDIO:
      return PREF_AUDIO_SELECTED_READER;

    default:
      return PREF_SELECTED_READER;
  }
}

function getPrefWebForType(t) {
  switch (t) {
    case Ci.nsIFeed.TYPE_VIDEO:
      return PREF_VIDEO_SELECTED_WEB;

    case Ci.nsIFeed.TYPE_AUDIO:
      return PREF_AUDIO_SELECTED_WEB;

    default:
      return PREF_SELECTED_WEB;
  }
}

function getPrefAppForType(t) {
  switch (t) {
    case Ci.nsIFeed.TYPE_VIDEO:
      return PREF_VIDEO_SELECTED_APP;

    case Ci.nsIFeed.TYPE_AUDIO:
      return PREF_AUDIO_SELECTED_APP;

    default:
      return PREF_SELECTED_APP;
  }
}

/**
 * Maps a feed type to a maybe-feed mimetype.
 */
function getMimeTypeForFeedType(aFeedType) {
  switch (aFeedType) {
    case Ci.nsIFeed.TYPE_VIDEO:
      return TYPE_MAYBE_VIDEO_FEED;

    case Ci.nsIFeed.TYPE_AUDIO:
      return TYPE_MAYBE_AUDIO_FEED;

    default:
      return TYPE_MAYBE_FEED;
  }
}

/**
 * The Feed Handler object manages discovery of RSS/ATOM feeds in web pages
 * and shows UI when they are discovered.
 */
var FeedHandler = {
  _prefChangeCallback: null,

  /** Called when the user clicks on the Subscribe to This Page... menu item,
   * or when the user clicks the feed button when the page contains multiple
   * feeds.
   * Builds a menu of unique feeds associated with the page, and if there
   * is only one, shows the feed inline in the browser window.
   * @param   container
   *          The feed list container (menupopup or subview) to be populated.
   * @param   isSubview
   *          Whether we're creating a subview (true) or menu (false/undefined)
   * @return  true if the menu/subview should be shown, false if there was only
   *          one feed and the feed should be shown inline in the browser
   *          window (do not show the menupopup/subview).
   */
  buildFeedList(container, isSubview) {
    let feeds = gBrowser.selectedBrowser.feeds;
    if (!isSubview && feeds == null) {
      // XXX hack -- menu opening depends on setting of an "open"
      // attribute, and the menu refuses to open if that attribute is
      // set (because it thinks it's already open).  onpopupshowing gets
      // called after the attribute is unset, and it doesn't get unset
      // if we return false.  so we unset it here; otherwise, the menu
      // refuses to work past this point.
      container.parentNode.removeAttribute("open");
      return false;
    }

    for (let i = container.childNodes.length - 1; i >= 0; --i) {
      let node = container.childNodes[i];
      if (isSubview && node.localName == "label")
        continue;
      container.removeChild(node);
    }

    if (!feeds || feeds.length <= 1)
      return false;

    // Build the menu showing the available feed choices for viewing.
    let itemNodeType = isSubview ? "toolbarbutton" : "menuitem";
    for (let feedInfo of feeds) {
      let item = document.createElement(itemNodeType);
      let baseTitle = feedInfo.title || feedInfo.href;
      item.setAttribute("label", baseTitle);
      item.setAttribute("feed", feedInfo.href);
      item.setAttribute("tooltiptext", feedInfo.href);
      item.setAttribute("crop", "center");
      let className = "feed-" + itemNodeType;
      if (isSubview) {
        className += " subviewbutton";
      }
      item.setAttribute("class", className);
      container.appendChild(item);
    }
    return true;
  },

  /**
   * Subscribe to a given feed.  Called when
   *   1. Page has a single feed and user clicks feed icon in location bar
   *   2. Page has a single feed and user selects Subscribe menu item
   *   3. Page has multiple feeds and user selects from feed icon popup (or subview)
   *   4. Page has multiple feeds and user selects from Subscribe submenu
   * @param   href
   *          The feed to subscribe to. May be null, in which case the
   *          event target's feed attribute is examined.
   * @param   event
   *          The event this method is handling. Used to decide where
   *          to open the preview UI. (Optional, unless href is null)
   */
  subscribeToFeed(href, event) {
    // Just load the feed in the content area to either subscribe or show the
    // preview UI
    if (!href)
      href = event.target.getAttribute("feed");
    urlSecurityCheck(href, gBrowser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
    this.loadFeed(href, event);
  },

  loadFeed(href, event) {
    let feeds = gBrowser.selectedBrowser.feeds;
    try {
      openUILink(href, event, { ignoreAlt: true });
    } finally {
      // We might default to a livebookmarks modal dialog,
      // so reset that if the user happens to click it again
      gBrowser.selectedBrowser.feeds = feeds;
    }
  },

  get _feedMenuitem() {
    delete this._feedMenuitem;
    return this._feedMenuitem = document.getElementById("singleFeedMenuitemState");
  },

  get _feedMenupopup() {
    delete this._feedMenupopup;
    return this._feedMenupopup = document.getElementById("multipleFeedsMenuState");
  },

  /**
   * Update the browser UI to show whether or not feeds are available when
   * a page is loaded or the user switches tabs to a page that has feeds.
   */
  updateFeeds() {
    if (this._updateFeedTimeout)
      clearTimeout(this._updateFeedTimeout);

    let feeds = gBrowser.selectedBrowser.feeds;
    let haveFeeds = feeds && feeds.length > 0;

    let feedButton = document.getElementById("feed-button");
    if (feedButton) {
      if (haveFeeds) {
        feedButton.removeAttribute("disabled");
      } else {
        feedButton.setAttribute("disabled", "true");
      }
    }

    if (!haveFeeds) {
      this._feedMenuitem.setAttribute("disabled", "true");
      this._feedMenuitem.removeAttribute("hidden");
      this._feedMenupopup.setAttribute("hidden", "true");
      return;
    }

    if (feeds.length > 1) {
      this._feedMenuitem.setAttribute("hidden", "true");
      this._feedMenupopup.removeAttribute("hidden");
    } else {
      this._feedMenuitem.setAttribute("feed", feeds[0].href);
      this._feedMenuitem.removeAttribute("disabled");
      this._feedMenuitem.removeAttribute("hidden");
      this._feedMenupopup.setAttribute("hidden", "true");
    }
  },

  addFeed(link, browserForLink) {
    if (!browserForLink.feeds)
      browserForLink.feeds = [];

    urlSecurityCheck(link.href, gBrowser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);

    let feedURI = makeURI(link.href, document.characterSet);
    if (!/^https?$/.test(feedURI.scheme))
      return;

    browserForLink.feeds.push({ href: link.href, title: link.title });

    // If this addition was for the current browser, update the UI. For
    // background browsers, we'll update on tab switch.
    if (browserForLink == gBrowser.selectedBrowser) {
      // Batch updates to avoid updating the UI for multiple onLinkAdded events
      // fired within 100ms of each other.
      if (this._updateFeedTimeout)
        clearTimeout(this._updateFeedTimeout);
      this._updateFeedTimeout = setTimeout(this.updateFeeds.bind(this), 100);
    }
  },

   /**
   * Get the human-readable display name of a file. This could be the
   * application name.
   * @param   file
   *          A nsIFile to look up the name of
   * @return  The display name of the application represented by the file.
   */
  _getFileDisplayName(file) {
    switch (AppConstants.platform) {
      case "win":
        if (file instanceof Ci.nsILocalFileWin) {
          try {
            return file.getVersionInfoField("FileDescription");
          } catch (e) {}
        }
        break;
      case "macosx":
        if (file instanceof Ci.nsILocalFileMac) {
          try {
            return file.bundleDisplayName;
          } catch (e) {}
        }
        break;
    }

    return file.leafName;
  },

  _chooseClientApp(aTitle, aTypeName, aBrowser) {
    const prefName = getPrefAppForType(aTypeName);
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

    fp.init(window, aTitle, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);

    fp.open((aResult) => {
      if (aResult == Ci.nsIFilePicker.returnOK) {
        let selectedApp = fp.file;
        if (selectedApp) {
          // XXXben - we need to compare this with the running instance
          //          executable just don't know how to do that via script
          // XXXmano TBD: can probably add this to nsIShellService
          let appName = "";
          switch (AppConstants.platform) {
            case "win":
              appName = AppConstants.MOZ_APP_NAME + ".exe";
              break;
            case "macosx":
              appName = AppConstants.MOZ_MACBUNDLE_NAME;
              break;
            default:
              appName = AppConstants.MOZ_APP_NAME + "-bin";
              break;
          }

          if (fp.file.leafName != appName) {
            Services.prefs.setComplexValue(prefName, Ci.nsIFile, selectedApp);
            aBrowser.messageManager.sendAsyncMessage("FeedWriter:SetApplicationLauncherMenuItem",
                                                    { name: this._getFileDisplayName(selectedApp),
                                                      type: "SelectedAppMenuItem" });
          }
        }
      }
    });

  },

  executeClientApp(aSpec, aTitle, aSubtitle, aFeedHandler) {
    // aFeedHandler is either "default", indicating the system default reader, or a pref-name containing
    // an nsIFile pointing to the feed handler's executable.

    let clientApp = null;
    if (aFeedHandler == "default") {
      clientApp = Cc["@mozilla.org/browser/shell-service;1"]
                    .getService(Ci.nsIShellService)
                    .defaultFeedReader;
    } else {
      clientApp = Services.prefs.getComplexValue(aFeedHandler, Ci.nsIFile);
    }

    // For the benefit of applications that might know how to deal with more
    // URLs than just feeds, send feed: URLs in the following format:
    //
    // http urls: replace scheme with feed, e.g.
    // http://foo.com/index.rdf -> feed://foo.com/index.rdf
    // other urls: prepend feed: scheme, e.g.
    // https://foo.com/index.rdf -> feed:https://foo.com/index.rdf
    let feedURI = NetUtil.newURI(aSpec);
    if (feedURI.schemeIs("http")) {
      feedURI.scheme = "feed";
      aSpec = feedURI.spec;
    } else {
      aSpec = "feed:" + aSpec;
    }

    // Retrieving the shell service might fail on some systems, most
    // notably systems where GNOME is not installed.
    try {
      let ss = Cc["@mozilla.org/browser/shell-service;1"]
                 .getService(Ci.nsIShellService);
      ss.openApplicationWithURI(clientApp, aSpec);
    } catch (e) {
      // If we couldn't use the shell service, fallback to using a
      // nsIProcess instance
      let p = Cc["@mozilla.org/process/util;1"]
                .createInstance(Ci.nsIProcess);
      p.init(clientApp);
      p.run(false, [aSpec], 1);
    }
  },

  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),


  init() {
    window.messageManager.addMessageListener("FeedWriter:ChooseClientApp", this);
    window.messageManager.addMessageListener("FeedWriter:GetSubscriptionUI", this);
    window.messageManager.addMessageListener("FeedWriter:SetFeedPrefsAndSubscribe", this);
    window.messageManager.addMessageListener("FeedWriter:ShownFirstRun", this);

    Services.ppmm.addMessageListener("FeedConverter:ExecuteClientApp", this);

    const prefs = Services.prefs;
    prefs.addObserver(PREF_SELECTED_ACTION, this, true);
    prefs.addObserver(PREF_SELECTED_READER, this, true);
    prefs.addObserver(PREF_SELECTED_WEB, this, true);
    prefs.addObserver(PREF_VIDEO_SELECTED_ACTION, this, true);
    prefs.addObserver(PREF_VIDEO_SELECTED_READER, this, true);
    prefs.addObserver(PREF_VIDEO_SELECTED_WEB, this, true);
    prefs.addObserver(PREF_AUDIO_SELECTED_ACTION, this, true);
    prefs.addObserver(PREF_AUDIO_SELECTED_READER, this, true);
    prefs.addObserver(PREF_AUDIO_SELECTED_WEB, this, true);
  },

  uninit() {
    Services.ppmm.removeMessageListener("FeedConverter:ExecuteClientApp", this);

    this._prefChangeCallback = null;
  },

  // nsIObserver
  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      LOG(`Pref changed ${data}`);
      if (this._prefChangeCallback) {
        this._prefChangeCallback.disarm();
      }
      // Multiple prefs are set at the same time, debounce to reduce noise
      // This can happen in one feed and we want to message all feed pages
      this._prefChangeCallback = new DeferredTask(() => {
        this._prefChanged(data);
      }, PREF_UPDATE_DELAY);
      this._prefChangeCallback.arm();
    }
  },

  _prefChanged(prefName) {
    // Don't observe for PREF_*SELECTED_APP as user likely just picked one
    // That is also handled by SetApplicationLauncherMenuItem call
    // Rather than the others which happen on subscription
    switch (prefName) {
      case PREF_SELECTED_READER:
      case PREF_SELECTED_WEB:
      case PREF_VIDEO_SELECTED_READER:
      case PREF_VIDEO_SELECTED_WEB:
      case PREF_AUDIO_SELECTED_READER:
      case PREF_AUDIO_SELECTED_WEB:
      case PREF_SELECTED_ACTION:
      case PREF_VIDEO_SELECTED_ACTION:
      case PREF_AUDIO_SELECTED_ACTION:
        const response = {
         default: this._getReaderForType(Ci.nsIFeed.TYPE_FEED),
         [Ci.nsIFeed.TYPE_AUDIO]: this._getReaderForType(Ci.nsIFeed.TYPE_AUDIO),
         [Ci.nsIFeed.TYPE_VIDEO]: this._getReaderForType(Ci.nsIFeed.TYPE_VIDEO)
        };
        Services.mm.broadcastAsyncMessage("FeedWriter:PreferenceUpdated",
                                          response);
        break;
    }
  },

  _initSubscriptionUIResponse(feedType) {
    const wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
               getService(Ci.nsIWebContentConverterService);
    const handlersRaw = wccr.getContentHandlers(getMimeTypeForFeedType(feedType));
    const handlers = [];
    for (let handler of handlersRaw) {
      LOG(`Handler found: ${handler}`);
      handlers.push({
        name: handler.name,
        uri: handler.uri
      });
    }
    let showFirstRunUI = true;
    // eslint-disable-next-line mozilla/use-default-preference-values
    try {
      showFirstRunUI = Services.prefs.getBoolPref(PREF_SHOW_FIRST_RUN_UI);
    } catch (ex) { }
    const response = { handlers, showFirstRunUI };
    let selectedClientApp;
    const feedTypePref = getPrefAppForType(feedType);
    try {
      selectedClientApp = Services.prefs.getComplexValue(feedTypePref, Ci.nsIFile);
    } catch (ex) {
      // Just do nothing, then we won't bother populating
    }

    let defaultClientApp = null;
    try {
      // This can sometimes not exist
      defaultClientApp = Cc["@mozilla.org/browser/shell-service;1"]
                           .getService(Ci.nsIShellService)
                           .defaultFeedReader;
    } catch (ex) {
      // Just do nothing, then we don't bother populating
    }

    if (selectedClientApp && selectedClientApp.exists()) {
      if (defaultClientApp && selectedClientApp.path != defaultClientApp.path) {
        // Only set the default menu item if it differs from the selected one
        response.defaultMenuItem = this._getFileDisplayName(defaultClientApp);
      }
      response.selectedMenuItem = this._getFileDisplayName(selectedClientApp);
    }
    response.reader = this._getReaderForType(feedType);
    return response;
  },

  _setPref(aPrefName, aPrefValue, aIsComplex = false) {
    LOG(`FeedWriter._setPref ${aPrefName}`);
    // Ensure we have a pref that is settable
    if (aPrefName && SETTABLE_PREFS.has(aPrefName)) {
      if (aIsComplex) {
        Services.prefs.setStringPref(aPrefName, aPrefValue);
      } else {
        Services.prefs.setCharPref(aPrefName, aPrefValue);
      }
    } else {
      LOG(`FeedWriter._setPref ${aPrefName} not allowed`);
    }
  },

  _getReaderForType(feedType) {
    let prefs = Services.prefs;
    let handler = "bookmarks";
    let url;
    // eslint-disable-next-line mozilla/use-default-preference-values
    try {
      handler = prefs.getCharPref(getPrefReaderForType(feedType));
    } catch (ex) { }

    if (handler === "web") {
      try {
        url = prefs.getStringPref(getPrefWebForType(feedType));
      } catch (ex) {
        LOG("FeedWriter._setSelectedHandler: invalid or no handler in prefs");
        url = null;
      }
    }
    const alwaysUse = this._getAlwaysUseState(feedType);
    const action = prefs.getCharPref(getPrefActionForType(feedType));
    return { handler, url, alwaysUse, action };
  },

  _getAlwaysUseState(feedType) {
    try {
      return Services.prefs.getCharPref(getPrefActionForType(feedType)) != "ask";
    } catch (ex) { }
    return false;
  },

  receiveMessage(msg) {
    let handler;
    switch (msg.name) {
      case "FeedWriter:GetSubscriptionUI":
        const response = this._initSubscriptionUIResponse(msg.data.feedType);
        msg.target.messageManager
           .sendAsyncMessage("FeedWriter:GetSubscriptionUIResponse",
                            response);
        break;
      case "FeedWriter:ChooseClientApp":
        this._chooseClientApp(msg.data.title, msg.data.feedType, msg.target);
        break;
      case "FeedWriter:ShownFirstRun":
        Services.prefs.setBoolPref(PREF_SHOW_FIRST_RUN_UI, false);
        break;
      case "FeedWriter:SetFeedPrefsAndSubscribe":
        const settings = msg.data;
        if (!settings.action || !VALID_ACTIONS.has(settings.action)) {
          LOG(`Invalid action ${settings.action}`);
          return;
        }
        if (!settings.reader || !VALID_READERS.has(settings.reader)) {
          LOG(`Invalid reader ${settings.reader}`);
          return;
        }
        const actionPref = getPrefActionForType(settings.feedType);
        this._setPref(actionPref, settings.action);
        const readerPref = getPrefReaderForType(settings.feedType);
        this._setPref(readerPref, settings.reader);
        handler = null;

        switch (settings.reader) {
          case "web":
            // This is a web set URI by content using window.registerContentHandler()
            // Lets make sure we know about it before setting it
            const webPref = getPrefWebForType(settings.feedType);
            let wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                       getService(Ci.nsIWebContentConverterService);
            // If the user provided an invalid web URL this function won't give us a reference
            handler = wccr.getWebContentHandlerByURI(getMimeTypeForFeedType(settings.feedType), settings.uri);
            if (handler) {
              this._setPref(webPref, settings.uri, true);
              if (settings.useAsDefault) {
                wccr.setAutoHandler(getMimeTypeForFeedType(settings.feedType), handler);
              }
              msg.target.messageManager
                 .sendAsyncMessage("FeedWriter:SetFeedPrefsAndSubscribeResponse",
                                  { redirect: handler.getHandlerURI(settings.feedLocation) });
            } else {
              LOG(`No handler found for web ${settings.feedType} ${settings.uri}`);
            }
            break;
          default:
            const feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                                getService(Ci.nsIFeedResultService);

            feedService.addToClientReader(settings.feedLocation,
                                          settings.feedTitle,
                                          settings.feedSubtitle,
                                          settings.feedType,
                                          settings.reader);
         }
         break;
      case "FeedConverter:ExecuteClientApp":
        // Always check feedHandler is from a set array of executable prefs
        if (EXECUTABLE_PREFS.has(msg.data.feedHandler)) {
          this.executeClientApp(msg.data.spec, msg.data.title,
                                msg.data.subtitle, msg.data.feedHandler);
        } else {
          LOG(`FeedConverter:ExecuteClientApp - Will not exec ${msg.data.feedHandler}`);
        }
        break;
    }
  },
};
