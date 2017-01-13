/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

const FEEDWRITER_CID = Components.ID("{49bb6593-3aff-4eb3-a068-2712c28bd58e}");
const FEEDWRITER_CONTRACTID = "@mozilla.org/browser/feeds/result-writer;1";

function LOG(str) {
  let prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);

  let shouldLog = false;
  try {
    shouldLog = prefB.getBoolPref("feeds.log");
  } catch (ex) {
  }

  if (shouldLog)
    dump("*** Feeds: " + str + "\n");
}

/**
 * Wrapper function for nsIIOService::newURI.
 * @param aURLSpec
 *        The URL string from which to create an nsIURI.
 * @returns an nsIURI object, or null if the creation of the URI failed.
 */
function makeURI(aURLSpec, aCharset) {
  let ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  try {
    return ios.newURI(aURLSpec, aCharset);
  } catch (ex) { }

  return null;
}

const XML_NS = "http://www.w3.org/XML/1998/namespace";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_MAYBE_AUDIO_FEED = "application/vnd.mozilla.maybe.audio.feed";
const TYPE_MAYBE_VIDEO_FEED = "application/vnd.mozilla.maybe.video.feed";
const URI_BUNDLE = "chrome://browser/locale/feeds/subscribe.properties";

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

const PREF_SHOW_FIRST_RUN_UI = "browser.feeds.showFirstRunUI";

const TITLE_ID = "feedTitleText";
const SUBTITLE_ID = "feedSubtitleText";

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

/**
 * Converts a number of bytes to the appropriate unit that results in a
 * number that needs fewer than 4 digits
 *
 * @return a pair: [new value with 3 sig. figs., its unit]
  */
function convertByteUnits(aBytes) {
  let units = ["bytes", "kilobyte", "megabyte", "gigabyte"];
  let unitIndex = 0;

  // convert to next unit if it needs 4 digits (after rounding), but only if
  // we know the name of the next unit
  while ((aBytes >= 999.5) && (unitIndex < units.length - 1)) {
    aBytes /= 1024;
    unitIndex++;
  }

  // Get rid of insignificant bits by truncating to 1 or 0 decimal points
  // 0 -> 0; 1.2 -> 1.2; 12.3 -> 12.3; 123.4 -> 123; 234.5 -> 235
  aBytes = aBytes.toFixed((aBytes > 0) && (aBytes < 100) ? 1 : 0);

  return [aBytes, units[unitIndex]];
}

function FeedWriter() {
  this._selectedApp = undefined;
  this._selectedAppMenuItem = null;
  this._subscribeCallback = null;
  this._defaultHandlerMenuItem = null;
}

FeedWriter.prototype = {
  _mimeSvc      : Cc["@mozilla.org/mime;1"].
                  getService(Ci.nsIMIMEService),

  _getPropertyAsBag(container, property) {
    return container.fields.getProperty(property).
                     QueryInterface(Ci.nsIPropertyBag2);
  },

  _getPropertyAsString(container, property) {
    try {
      return container.fields.getPropertyAsAString(property);
    } catch (e) {
    }
    return "";
  },

  _setContentText(id, text) {
    let element = this._document.getElementById(id);
    let textNode = text.createDocumentFragment(element);
    while (element.hasChildNodes())
      element.removeChild(element.firstChild);
    element.appendChild(textNode);
    if (text.base) {
      element.setAttributeNS(XML_NS, 'base', text.base.spec);
    }
  },

  /**
   * Safely sets the href attribute on an anchor tag, providing the URI
   * specified can be loaded according to rules.
   * @param   element
   *          The element to set a URI attribute on
   * @param   attribute
   *          The attribute of the element to set the URI to, e.g. href or src
   * @param   uri
   *          The URI spec to set as the href
   */
  _safeSetURIAttribute(element, attribute, uri) {
    let secman = Cc["@mozilla.org/scriptsecuritymanager;1"].
                 getService(Ci.nsIScriptSecurityManager);
    const flags = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;
    try {
      // TODO Is this necessary?
      secman.checkLoadURIStrWithPrincipal(this._feedPrincipal, uri, flags);
      // checkLoadURIStrWithPrincipal will throw if the link URI should not be
      // loaded, either because our feedURI isn't allowed to load it or per
      // the rules specified in |flags|, so we'll never "linkify" the link...
    } catch (e) {
      // Not allowed to load this link because secman.checkLoadURIStr threw
      return;
    }

    element.setAttribute(attribute, uri);
  },

  __bundle: null,
  get _bundle() {
    if (!this.__bundle) {
      this.__bundle = Cc["@mozilla.org/intl/stringbundle;1"].
                      getService(Ci.nsIStringBundleService).
                      createBundle(URI_BUNDLE);
    }
    return this.__bundle;
  },

  _getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },

  _getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  _setCheckboxCheckedState(aCheckbox, aValue) {
    // see checkbox.xml, xbl bindings are not applied within the sandbox! TODO
    let change = (aValue != (aCheckbox.getAttribute('checked') == 'true'));
    if (aValue)
      aCheckbox.setAttribute('checked', 'true');
    else
      aCheckbox.removeAttribute('checked');

    if (change) {
      let event = this._document.createEvent('Events');
      event.initEvent('CheckboxStateChange', true, true);
      aCheckbox.dispatchEvent(event);
    }
  },

   /**
   * Returns a date suitable for displaying in the feed preview.
   * If the date cannot be parsed, the return value is "false".
   * @param   dateString
   *          A date as extracted from a feed entry. (entry.updated)
   */
  _parseDate(dateString) {
    // Convert the date into the user's local time zone
    let dateObj = new Date(dateString);

    // Make sure the date we're given is valid.
    if (!dateObj.getTime())
      return false;

    return this._dateFormatter.format(dateObj);
  },

  __dateFormatter: null,
  get _dateFormatter() {
    if (!this.__dateFormatter) {
      const locale = Cc["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Ci.nsIXULChromeRegistry)
                     .getSelectedLocale("global", true);
      const dtOptions = { year: 'numeric', month: 'long', day: 'numeric',
                          hour: 'numeric', minute: 'numeric' };
      this.__dateFormatter = new Intl.DateTimeFormat(locale, dtOptions);
    }
    return this.__dateFormatter;
  },

  /**
   * Returns the feed type.
   */
  __feedType: null,
  _getFeedType() {
    if (this.__feedType != null)
      return this.__feedType;

    try {
      // grab the feed because it's got the feed.type in it.
      let container = this._getContainer();
      let feed = container.QueryInterface(Ci.nsIFeed);
      this.__feedType = feed.type;
      return feed.type;
    } catch (ex) { }

    return Ci.nsIFeed.TYPE_FEED;
  },

  /**
   * Maps a feed type to a maybe-feed mimetype.
   */
  _getMimeTypeForFeedType() {
    switch (this._getFeedType()) {
      case Ci.nsIFeed.TYPE_VIDEO:
        return TYPE_MAYBE_VIDEO_FEED;

      case Ci.nsIFeed.TYPE_AUDIO:
        return TYPE_MAYBE_AUDIO_FEED;

      default:
        return TYPE_MAYBE_FEED;
    }
  },

  /**
   * Writes the feed title into the preview document.
   * @param   container
   *          The feed container
   */
  _setTitleText(container) {
    if (container.title) {
      let title = container.title.plainText();
      this._setContentText(TITLE_ID, container.title);
      this._document.title = title;
    }

    let feed = container.QueryInterface(Ci.nsIFeed);
    if (feed && feed.subtitle)
      this._setContentText(SUBTITLE_ID, container.subtitle);
  },

  /**
   * Writes the title image into the preview document if one is present.
   * @param   container
   *          The feed container
   */
  _setTitleImage(container) {
    try {
      let parts = container.image;

      // Set up the title image (supplied by the feed)
      let feedTitleImage = this._document.getElementById("feedTitleImage");
      this._safeSetURIAttribute(feedTitleImage, "src",
                                parts.getPropertyAsAString("url"));

      // Set up the title image link
      let feedTitleLink = this._document.getElementById("feedTitleLink");

      let titleText = this._getFormattedString("linkTitleTextFormat",
                                               [parts.getPropertyAsAString("title")]);
      let feedTitleText = this._document.getElementById("feedTitleText");
      let titleImageWidth = parseInt(parts.getPropertyAsAString("width")) + 15;

      // Fix the margin on the main title, so that the image doesn't run over
      // the underline
      feedTitleLink.setAttribute('title', titleText);
      feedTitleText.style.marginRight = titleImageWidth + 'px';

      this._safeSetURIAttribute(feedTitleLink, "href",
                                parts.getPropertyAsAString("link"));
    } catch (e) {
      LOG("Failed to set Title Image (this is benign): " + e);
    }
  },

  /**
   * Writes all entries contained in the feed.
   * @param   container
   *          The container of entries in the feed
   */
  _writeFeedContent(container) {
    // Build the actual feed content
    let feed = container.QueryInterface(Ci.nsIFeed);
    if (feed.items.length == 0)
      return;

    let feedContent = this._document.getElementById("feedContent");

    for (let i = 0; i < feed.items.length; ++i) {
      let entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      entry.QueryInterface(Ci.nsIFeedContainer);

      let entryContainer = this._document.createElementNS(HTML_NS, "div");
      entryContainer.className = "entry";

      // If the entry has a title, make it a link
      if (entry.title) {
        let a = this._document.createElementNS(HTML_NS, "a");
        let span = this._document.createElementNS(HTML_NS, "span");
        a.appendChild(span);
        if (entry.title.base)
          span.setAttributeNS(XML_NS, "base", entry.title.base.spec);
        span.appendChild(entry.title.createDocumentFragment(a));

        // Entries are not required to have links, so entry.link can be null.
        if (entry.link)
          this._safeSetURIAttribute(a, "href", entry.link.spec);

        let title = this._document.createElementNS(HTML_NS, "h3");
        title.appendChild(a);

        let lastUpdated = this._parseDate(entry.updated);
        if (lastUpdated) {
          let dateDiv = this._document.createElementNS(HTML_NS, "div");
          dateDiv.className = "lastUpdated";
          dateDiv.textContent = lastUpdated;
          title.appendChild(dateDiv);
        }

        entryContainer.appendChild(title);
      }

      let body = this._document.createElementNS(HTML_NS, "div");
      let summary = entry.summary || entry.content;
      let docFragment = null;
      if (summary) {
        if (summary.base)
          body.setAttributeNS(XML_NS, "base", summary.base.spec);
        else
          LOG("no base?");
        docFragment = summary.createDocumentFragment(body);
        if (docFragment)
          body.appendChild(docFragment);

        // If the entry doesn't have a title, append a # permalink
        // See http://scripting.com/rss.xml for an example
        if (!entry.title && entry.link) {
          let a = this._document.createElementNS(HTML_NS, "a");
          a.appendChild(this._document.createTextNode("#"));
          this._safeSetURIAttribute(a, "href", entry.link.spec);
          body.appendChild(this._document.createTextNode(" "));
          body.appendChild(a);
        }

      }
      body.className = "feedEntryContent";
      entryContainer.appendChild(body);

      if (entry.enclosures && entry.enclosures.length > 0) {
        let enclosuresDiv = this._buildEnclosureDiv(entry);
        entryContainer.appendChild(enclosuresDiv);
      }

      let clearDiv = this._document.createElementNS(HTML_NS, "div");
      clearDiv.style.clear = "both";

      feedContent.appendChild(entryContainer);
      feedContent.appendChild(clearDiv);
    }
  },

  /**
   * Takes a url to a media item and returns the best name it can come up with.
   * Frequently this is the filename portion (e.g. passing in
   * http://example.com/foo.mpeg would return "foo.mpeg"), but in more complex
   * cases, this will return the entire url (e.g. passing in
   * http://example.com/somedirectory/ would return
   * http://example.com/somedirectory/).
   * @param aURL
   *        The URL string from which to create a display name
   * @returns a string
   */
  _getURLDisplayName(aURL) {
    let url = makeURI(aURL);
    url.QueryInterface(Ci.nsIURL);
    if (url == null || url.fileName.length == 0)
      return decodeURIComponent(aURL);

    return decodeURIComponent(url.fileName);
  },

  /**
   * Takes a FeedEntry with enclosures, generates the HTML code to represent
   * them, and returns that.
   * @param   entry
   *          FeedEntry with enclosures
   * @returns element
   */
  _buildEnclosureDiv(entry) {
    let enclosuresDiv = this._document.createElementNS(HTML_NS, "div");
    enclosuresDiv.className = "enclosures";

    enclosuresDiv.appendChild(this._document.createTextNode(this._getString("mediaLabel")));

    for (let i_enc = 0; i_enc < entry.enclosures.length; ++i_enc) {
      let enc = entry.enclosures.queryElementAt(i_enc, Ci.nsIWritablePropertyBag2);

      if (!(enc.hasKey("url")))
        continue;

      let enclosureDiv = this._document.createElementNS(HTML_NS, "div");
      enclosureDiv.setAttribute("class", "enclosure");

      let mozicon = "moz-icon://.txt?size=16";
      let type_text = null;
      let size_text = null;

      if (enc.hasKey("type")) {
        type_text = enc.get("type");
        try {
          let handlerInfoWrapper = this._mimeSvc.getFromTypeAndExtension(enc.get("type"), null);

          if (handlerInfoWrapper)
            type_text = handlerInfoWrapper.description;

          if (type_text && type_text.length > 0)
            mozicon = "moz-icon://goat?size=16&contentType=" + enc.get("type");

        } catch (ex) { }

      }

      if (enc.hasKey("length") && /^[0-9]+$/.test(enc.get("length"))) {
        let enc_size = convertByteUnits(parseInt(enc.get("length")));

        size_text = this._getFormattedString("enclosureSizeText",
                                             [enc_size[0],
                                             this._getString(enc_size[1])]);
      }

      let iconimg = this._document.createElementNS(HTML_NS, "img");
      iconimg.setAttribute("src", mozicon);
      iconimg.setAttribute("class", "type-icon");
      enclosureDiv.appendChild(iconimg);

      enclosureDiv.appendChild(this._document.createTextNode( " " ));

      let enc_href = this._document.createElementNS(HTML_NS, "a");
      enc_href.appendChild(this._document.createTextNode(this._getURLDisplayName(enc.get("url"))));
      this._safeSetURIAttribute(enc_href, "href", enc.get("url"));
      enclosureDiv.appendChild(enc_href);

      if (type_text && size_text)
        enclosureDiv.appendChild(this._document.createTextNode( " (" + type_text + ", " + size_text + ")"));

      else if (type_text)
        enclosureDiv.appendChild(this._document.createTextNode( " (" + type_text + ")"))

      else if (size_text)
        enclosureDiv.appendChild(this._document.createTextNode( " (" + size_text + ")"))

      enclosuresDiv.appendChild(enclosureDiv);
    }

    return enclosuresDiv;
  },

  /**
   * Gets a valid nsIFeedContainer object from the parsed nsIFeedResult.
   * Displays error information if there was one.
   * @returns A valid nsIFeedContainer object containing the contents of
   *          the feed.
   */
  _getContainer() {
    let feedService =
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);

    let result;
    try {
      result =
        feedService.getFeedResult(this._getOriginalURI(this._window));
    } catch (e) {
      LOG("Subscribe Preview: feed not available?!");
    }

    if (result.bozo) {
      LOG("Subscribe Preview: feed result is bozo?!");
    }

    let container;
    try {
      container = result.doc;
    } catch (e) {
      LOG("Subscribe Preview: no result.doc? Why didn't the original reload?");
      return null;
    }
    return container;
  },

  /**
   * Get moz-icon url for a file
   * @param   file
   *          A nsIFile object for which the moz-icon:// is returned
   * @returns moz-icon url of the given file as a string
   */
  _getFileIconURL(file) {
    let ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    let fph = ios.getProtocolHandler("file")
                 .QueryInterface(Ci.nsIFileProtocolHandler);
    let urlSpec = fph.getURLSpecFromFile(file);
    return "moz-icon://" + urlSpec + "?size=16";
  },

  /**
   * Displays a prompt from which the user may choose a (client) feed reader.
   * @param aCallback the callback method, passes in true if a feed reader was
   *        selected, false otherwise.
   */
  _chooseClientApp(aCallback) {
    this._subscribeCallback = aCallback;
    this._mm.sendAsyncMessage("FeedWriter:ChooseClientApp",
                              { title: this._getString("chooseApplicationDialogTitle"),
                                prefName: getPrefAppForType(this._getFeedType()) });
  },

  _setAlwaysUseCheckedState(feedType) {
    let checkbox = this._document.getElementById("alwaysUse");
    if (checkbox) {
      let alwaysUse = false;
      try {
        if (Services.prefs.getCharPref(getPrefActionForType(feedType)) != "ask")
          alwaysUse = true;
      } catch (ex) { }
      this._setCheckboxCheckedState(checkbox, alwaysUse);
    }
  },

  _setSubscribeUsingLabel() {
    let stringLabel = "subscribeFeedUsing";
    switch (this._getFeedType()) {
      case Ci.nsIFeed.TYPE_VIDEO:
        stringLabel = "subscribeVideoPodcastUsing";
        break;

      case Ci.nsIFeed.TYPE_AUDIO:
        stringLabel = "subscribeAudioPodcastUsing";
        break;
    }

    let subscribeUsing = this._document.getElementById("subscribeUsingDescription");
    let textNode = this._document.createTextNode(this._getString(stringLabel));
    subscribeUsing.insertBefore(textNode, subscribeUsing.firstChild);
  },

  _setAlwaysUseLabel() {
    let checkbox = this._document.getElementById("alwaysUse");
    if (checkbox && this._handlersList) {
      let handlerName = this._handlersList.selectedOptions[0]
                            .textContent;
      let stringLabel = "alwaysUseForFeeds";
      switch (this._getFeedType()) {
        case Ci.nsIFeed.TYPE_VIDEO:
          stringLabel = "alwaysUseForVideoPodcasts";
          break;

        case Ci.nsIFeed.TYPE_AUDIO:
          stringLabel = "alwaysUseForAudioPodcasts";
          break;
      }

      let label = this._getFormattedString(stringLabel, [handlerName]);

      let checkboxText = this._document.getElementById("checkboxText");
      if (checkboxText.lastChild.nodeType == checkboxText.TEXT_NODE) {
        checkboxText.lastChild.textContent = label;
      } else {
        LOG("FeedWriter._setAlwaysUseLabel: Expected textNode as lastChild of alwaysUse label");
        let textNode = this._document.createTextNode(label);
        checkboxText.appendChild(textNode);
      }
    }
  },

  // nsIDomEventListener
  handleEvent(event) {
    if (event.target.ownerDocument != this._document) {
      LOG("FeedWriter.handleEvent: Someone passed the feed writer as a listener to the events of another document!");
      return;
    }

    switch (event.type) {
      case "click":
        if (event.target.id == "subscribeButton") {
          this.subscribe();
        }
      break;
      case "change":
        if (event.target.selectedOptions[0].id == "chooseApplicationMenuItem") {
          this._chooseClientApp((aResult) => {
            if (!aResult) {
              // Select the (per-prefs) selected handler if no application
              // was selected
              this._setSelectedHandler(this._getFeedType());
            }
          });
        } else {
          this._setAlwaysUseLabel();
        }
        break;
    }
  },

  _getWebHandlerElementsForURL(aURL) {
    let menu = this._document.getElementById("handlersMenuList");
    return menu.querySelectorAll('[webhandlerurl="' + aURL + '"]');
  },

  _setSelectedHandler(feedType) {
    let prefs = Services.prefs;
    let handler = "bookmarks";
    try {
      handler = prefs.getCharPref(getPrefReaderForType(feedType));
    } catch (ex) { }

    switch (handler) {
      case "web": {
        if (this._handlersList) {
          let url;
          try {
            url = prefs.getComplexValue(getPrefWebForType(feedType), Ci.nsISupportsString).data;
          } catch (ex) {
            LOG("FeedWriter._setSelectedHandler: invalid or no handler in prefs");
            return;
          }
          let handlers =
            this._getWebHandlerElementsForURL(url);
          if (handlers.length == 0) {
            LOG("FeedWriter._setSelectedHandler: selected web handler isn't in the menulist")
            return;
          }

          handlers[0].selected = true;
        }
        break;
      }
      case "client":
      case "default":
        // do nothing, these are handled by the onchange event
        break;
      case "bookmarks":
      default: {
        let liveBookmarksMenuItem = this._document.getElementById("liveBookmarksMenuItem");
        if (liveBookmarksMenuItem)
          liveBookmarksMenuItem.selected = true;
      }
    }
  },

  _initSubscriptionUI() {
    let handlersList = this._document.getElementById("handlersMenuList");
    if (!handlersList)
      return;

    let feedType = this._getFeedType();

    // change the background
    let header = this._document.getElementById("feedHeader");
    switch (feedType) {
      case Ci.nsIFeed.TYPE_VIDEO:
        header.className = 'videoPodcastBackground';
        break;

      case Ci.nsIFeed.TYPE_AUDIO:
        header.className = 'audioPodcastBackground';
        break;

      default:
        header.className = 'feedBackground';
    }

    let liveBookmarksMenuItem = this._document.getElementById("liveBookmarksMenuItem");

    // Last-selected application
    let menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("id", "selectedAppMenuItem");
    menuItem.setAttribute("handlerType", "client");

    // Hide the menuitem until we select an app
    menuItem.style.display = "none";
    this._selectedAppMenuItem = menuItem;

    handlersList.appendChild(this._selectedAppMenuItem);

    // Create the menuitem for the default reader, but don't show/populate it until
    // we get confirmation of what it is from the parent
    menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("id", "defaultHandlerMenuItem");
    menuItem.setAttribute("handlerType", "client");
    menuItem.style.display = "none";

    this._defaultHandlerMenuItem = menuItem;
    handlersList.appendChild(this._defaultHandlerMenuItem);

    this._mm.sendAsyncMessage("FeedWriter:RequestClientAppName",
                              { feedTypePref: getPrefAppForType(feedType) });

    // "Choose Application..." menuitem
    menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("id", "chooseApplicationMenuItem");
    menuItem.textContent = this._getString("chooseApplicationMenuItem");

    handlersList.appendChild(menuItem);

    // separator
    let chooseAppSep = liveBookmarksMenuItem.nextElementSibling.cloneNode(false);
    chooseAppSep.textContent = liveBookmarksMenuItem.nextElementSibling.textContent;
    handlersList.appendChild(chooseAppSep);

    // List of web handlers
    let wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
               getService(Ci.nsIWebContentConverterService);
    let handlers = wccr.getContentHandlers(this._getMimeTypeForFeedType(feedType));
    for (let handler of handlers) {
      if (!handler.uri) {
        LOG("Handler with name " + handler.name + " has no URI!? Skipping...");
        continue;
      }
      menuItem = liveBookmarksMenuItem.cloneNode(false);
      menuItem.removeAttribute("selected");
      menuItem.className = "menuitem-iconic";
      menuItem.textContent = handler.name;
      menuItem.setAttribute("handlerType", "web");
      menuItem.setAttribute("webhandlerurl", handler.uri);
      handlersList.appendChild(menuItem);
    }

    this._setSelectedHandler(feedType);

    // "Subscribe using..."
    this._setSubscribeUsingLabel();

    // "Always use..." checkbox initial state
    this._setAlwaysUseCheckedState(feedType);
    this._setAlwaysUseLabel();

    // We update the "Always use.." checkbox label whenever the selected item
    // in the list is changed
    handlersList.addEventListener("change", this);

    // Set up the "Subscribe Now" button
    this._document.getElementById("subscribeButton")
        .addEventListener("click", this);

    // first-run ui
    let showFirstRunUI = true;
    try {
      showFirstRunUI = Services.prefs.getBoolPref(PREF_SHOW_FIRST_RUN_UI);
    } catch (ex) { }
    if (showFirstRunUI) {
      let textfeedinfo1, textfeedinfo2;
      switch (feedType) {
        case Ci.nsIFeed.TYPE_VIDEO:
          textfeedinfo1 = "feedSubscriptionVideoPodcast1";
          textfeedinfo2 = "feedSubscriptionVideoPodcast2";
          break;
        case Ci.nsIFeed.TYPE_AUDIO:
          textfeedinfo1 = "feedSubscriptionAudioPodcast1";
          textfeedinfo2 = "feedSubscriptionAudioPodcast2";
          break;
        default:
          textfeedinfo1 = "feedSubscriptionFeed1";
          textfeedinfo2 = "feedSubscriptionFeed2";
      }

      let feedinfo1 = this._document.getElementById("feedSubscriptionInfo1");
      let feedinfo1Str = this._getString(textfeedinfo1);
      let feedinfo2 = this._document.getElementById("feedSubscriptionInfo2");
      let feedinfo2Str = this._getString(textfeedinfo2);

      feedinfo1.textContent = feedinfo1Str;
      feedinfo2.textContent = feedinfo2Str;

      header.setAttribute('firstrun', 'true');

      this._mm.sendAsyncMessage("FeedWriter:ShownFirstRun");
    }
  },

  /**
   * Returns the original URI object of the feed and ensures that this
   * component is only ever invoked from the preview document.
   * @param aWindow
   *        The window of the document invoking the BrowserFeedWriter
   */
  _getOriginalURI(aWindow) {
    let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    let chan = docShell.currentDocumentChannel;

    // We probably need to call Inherit() for this, but right now we can't call
    // it from JS.
    let attrs = docShell.getOriginAttributes();
    let ssm = Services.scriptSecurityManager;
    let nullPrincipal = ssm.createNullPrincipal(attrs);

    // this channel is not going to be openend, use a nullPrincipal
    // and the most restrctive securityFlag.
    let resolvedURI = NetUtil.newChannel({
      uri: "about:feeds",
      loadingPrincipal: nullPrincipal,
      securityFlags: Ci.nsILoadInfo.SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    }).URI;

    if (resolvedURI.equals(chan.URI))
      return chan.originalURI;

    return null;
  },

  _window: null,
  _document: null,
  _feedURI: null,
  _feedPrincipal: null,
  _handlersList: null,

  // BrowserFeedWriter WebIDL methods
  init(aWindow) {
    let window = aWindow;
    this._feedURI = this._getOriginalURI(window);
    if (!this._feedURI)
      return;

    this._window = window;
    this._document = window.document;
    this._handlersList = this._document.getElementById("handlersMenuList");

    let secman = Cc["@mozilla.org/scriptsecuritymanager;1"].
                 getService(Ci.nsIScriptSecurityManager);
    this._feedPrincipal = secman.createCodebasePrincipal(this._feedURI, {});

    LOG("Subscribe Preview: feed uri = " + this._window.location.href);

    // Set up the subscription UI
    this._initSubscriptionUI();
    let prefs = Services.prefs;
    prefs.addObserver(PREF_SELECTED_ACTION, this, false);
    prefs.addObserver(PREF_SELECTED_READER, this, false);
    prefs.addObserver(PREF_SELECTED_WEB, this, false);
    prefs.addObserver(PREF_SELECTED_APP, this, false);
    prefs.addObserver(PREF_VIDEO_SELECTED_ACTION, this, false);
    prefs.addObserver(PREF_VIDEO_SELECTED_READER, this, false);
    prefs.addObserver(PREF_VIDEO_SELECTED_WEB, this, false);
    prefs.addObserver(PREF_VIDEO_SELECTED_APP, this, false);

    prefs.addObserver(PREF_AUDIO_SELECTED_ACTION, this, false);
    prefs.addObserver(PREF_AUDIO_SELECTED_READER, this, false);
    prefs.addObserver(PREF_AUDIO_SELECTED_WEB, this, false);
    prefs.addObserver(PREF_AUDIO_SELECTED_APP, this, false);

    this._mm.addMessageListener("FeedWriter:SetApplicationLauncherMenuItem", this);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "FeedWriter:SetApplicationLauncherMenuItem":
        let menuItem = null;

        if (msg.data.type == "DefaultAppMenuItem") {
          menuItem = this._defaultHandlerMenuItem;
        } else {
          // Most likely SelectedAppMenuItem
          menuItem = this._selectedAppMenuItem;
        }

        menuItem.textContent = msg.data.name;
        menuItem.style.display = "";
        menuItem.selected = true;

        // Potentially a bit racy, but I don't think we can get into a state where this callback is set and
        // we're not coming back from ChooseClientApp in browser-feeds.js
        if (this._subscribeCallback) {
          this._subscribeCallback();
          this._subscribeCallback = null;
        }

        break;
    }
  },

  writeContent() {
    if (!this._window)
      return;

    try {
      // Set up the feed content
      let container = this._getContainer();
      if (!container)
        return;

      this._setTitleText(container);
      this._setTitleImage(container);
      this._writeFeedContent(container);
    } finally {
      this._removeFeedFromCache();
    }
  },

  close() {
    this._document.getElementById("subscribeButton")
        .removeEventListener("click", this, false);
    this._document.getElementById("handlersMenuList")
        .removeEventListener("change", this, false);
    this._document = null;
    this._window = null;
    let prefs = Services.prefs;
    prefs.removeObserver(PREF_SELECTED_ACTION, this);
    prefs.removeObserver(PREF_SELECTED_READER, this);
    prefs.removeObserver(PREF_SELECTED_WEB, this);
    prefs.removeObserver(PREF_SELECTED_APP, this);
    prefs.removeObserver(PREF_VIDEO_SELECTED_ACTION, this);
    prefs.removeObserver(PREF_VIDEO_SELECTED_READER, this);
    prefs.removeObserver(PREF_VIDEO_SELECTED_WEB, this);
    prefs.removeObserver(PREF_VIDEO_SELECTED_APP, this);

    prefs.removeObserver(PREF_AUDIO_SELECTED_ACTION, this);
    prefs.removeObserver(PREF_AUDIO_SELECTED_READER, this);
    prefs.removeObserver(PREF_AUDIO_SELECTED_WEB, this);
    prefs.removeObserver(PREF_AUDIO_SELECTED_APP, this);

    this._removeFeedFromCache();
    this.__bundle = null;
    this._feedURI = null;

    this._selectedApp = undefined;
    this._selectedAppMenuItem = null;
    this._defaultHandlerMenuItem = null;
  },

  _removeFeedFromCache() {
    if (this._feedURI) {
      let feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                        getService(Ci.nsIFeedResultService);
      feedService.removeFeedResult(this._feedURI);
      this._feedURI = null;
    }
  },

  setFeedCharPref(aPrefName, aPrefValue) {
      this._mm.sendAsyncMessage("FeedWriter:SetFeedCharPref",
                                { pref: aPrefName,
                                  value: aPrefValue });
  },

  setFeedComplexString(aPrefName, aPrefValue) {
      // This sends the string data across to the parent, which will use it in an nsISupportsString
      // for a complex value pref.
      this._mm.sendAsyncMessage("FeedWriter:SetFeedComplexString",
                                { pref: aPrefName,
                                  value: aPrefValue });
  },

  subscribe() {
    let feedType = this._getFeedType();

    // Subscribe to the feed using the selected handler and save prefs
    let defaultHandler = "reader";
    let useAsDefault = this._document.getElementById("alwaysUse").getAttribute("checked");

    let menuList = this._document.getElementById("handlersMenuList");
    let selectedItem = menuList.selectedOptions[0];
    let subscribeCallback = () => {
      if (selectedItem.hasAttribute("webhandlerurl")) {
        let webURI = selectedItem.getAttribute("webhandlerurl");
        this.setFeedCharPref(getPrefReaderForType(feedType), "web");
        this.setFeedComplexString(getPrefWebForType(feedType), webURI);

        let wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                   getService(Ci.nsIWebContentConverterService);
        let handler = wccr.getWebContentHandlerByURI(this._getMimeTypeForFeedType(feedType), webURI);
        if (handler) {
          if (useAsDefault) {
            wccr.setAutoHandler(this._getMimeTypeForFeedType(feedType), handler);
          }

          this._window.location.href = handler.getHandlerURI(this._window.location.href);
        }
      } else {
        let feedReader = null;
        switch (selectedItem.id) {
          case "selectedAppMenuItem":
            feedReader = "client";
            break;
          case "defaultHandlerMenuItem":
            feedReader = "default";
            break;
          case "liveBookmarksMenuItem":
            defaultHandler = "bookmarks";
            feedReader = "bookmarks";
            break;
        }

        this.setFeedCharPref(getPrefReaderForType(feedType), feedReader);

        let feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                          getService(Ci.nsIFeedResultService);

        // Pull the title and subtitle out of the document
        let feedTitle = this._document.getElementById(TITLE_ID).textContent;
        let feedSubtitle = this._document.getElementById(SUBTITLE_ID).textContent;
        feedService.addToClientReader(this._window.location.href, feedTitle, feedSubtitle, feedType, feedReader);
      }

      // If "Always use..." is checked, we should set PREF_*SELECTED_ACTION
      // to either "reader" (If a web reader or if an application is selected),
      // or to "bookmarks" (if the live bookmarks option is selected).
      // Otherwise, we should set it to "ask"
      if (useAsDefault) {
        this.setFeedCharPref(getPrefActionForType(feedType), defaultHandler);
      } else {
        this.setFeedCharPref(getPrefActionForType(feedType), "ask");
      }
    }

    // Show the file picker before subscribing if the
    // choose application menuitem was chosen using the keyboard
    if (selectedItem.id == "chooseApplicationMenuItem") {
      this._chooseClientApp(function(aResult) {
        if (aResult) {
          selectedItem =
            this._handlersList.selectedOptions[0];
          subscribeCallback();
        }
      }.bind(this));
    } else {
      subscribeCallback();
    }
  },

  // nsIObserver
  observe(subject, topic, data) {
    if (!this._window) {
      // this._window is null unless this.init was called with a trusted
      // window object.
      return;
    }

    let feedType = this._getFeedType();

    if (topic == "nsPref:changed") {
      switch (data) {
        case PREF_SELECTED_READER:
        case PREF_SELECTED_WEB:
        case PREF_SELECTED_APP:
        case PREF_VIDEO_SELECTED_READER:
        case PREF_VIDEO_SELECTED_WEB:
        case PREF_VIDEO_SELECTED_APP:
        case PREF_AUDIO_SELECTED_READER:
        case PREF_AUDIO_SELECTED_WEB:
        case PREF_AUDIO_SELECTED_APP:
          this._setSelectedHandler(feedType);
          break;
        case PREF_SELECTED_ACTION:
        case PREF_VIDEO_SELECTED_ACTION:
        case PREF_AUDIO_SELECTED_ACTION:
          this._setAlwaysUseCheckedState(feedType);
      }
    }
  },

  get _mm() {
    let mm = this._window.QueryInterface(Ci.nsIInterfaceRequestor).
                          getInterface(Ci.nsIDocShell).
                          QueryInterface(Ci.nsIInterfaceRequestor).
                          getInterface(Ci.nsIContentFrameMessageManager);
    delete this._mm;
    return this._mm = mm;
  },

  classID: FEEDWRITER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMEventListener, Ci.nsIObserver,
                                         Ci.nsINavHistoryObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FeedWriter]);
