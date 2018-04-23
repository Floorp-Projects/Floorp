/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const FEEDWRITER_CID = Components.ID("{49bb6593-3aff-4eb3-a068-2712c28bd58e}");
const FEEDWRITER_CONTRACTID = "@mozilla.org/browser/feeds/result-writer;1";

function LOG(str) {
  let shouldLog = Services.prefs.getBoolPref("feeds.log", false);

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
  try {
    return Services.io.newURI(aURLSpec, aCharset);
  } catch (ex) { }

  return null;
}

const XML_NS = "http://www.w3.org/XML/1998/namespace";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const URI_BUNDLE = "chrome://browser/locale/feeds/subscribe.properties";

const TITLE_ID = "feedTitleText";
const SUBTITLE_ID = "feedSubtitleText";

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

XPCOMUtils.defineLazyPreferenceGetter(this, "gCanFrameFeeds",
  "browser.feeds.unsafelyFrameFeeds", false);

function FeedWriter() {
  this._selectedApp = undefined;
  this._selectedAppMenuItem = null;
  this._subscribeCallback = null;
  this._defaultHandlerMenuItem = null;

  Services.telemetry.scalarAdd("browser.feeds.preview_loaded", 1);

  XPCOMUtils.defineLazyGetter(this, "_mm", () =>
    this._window.QueryInterface(Ci.nsIInterfaceRequestor).
                 getInterface(Ci.nsIDocShell).
                 QueryInterface(Ci.nsIInterfaceRequestor).
                 getInterface(Ci.nsIContentFrameMessageManager));
}

FeedWriter.prototype = {
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
      element.firstChild.remove();
    element.appendChild(textNode);
    if (text.base) {
      element.setAttributeNS(XML_NS, "base", text.base.spec);
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
    const flags = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;
    try {
      // TODO Is this necessary?
      Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(this._feedPrincipal, uri, flags);
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
      this.__bundle = Services.strings.createBundle(URI_BUNDLE);
    }
    return this.__bundle;
  },

  _getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },

  _getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  _setCheckboxCheckedState(aValue) {
    let checkbox = this._document.getElementById("alwaysUse");
    if (checkbox) {
      // see checkbox.xml, xbl bindings are not applied within the sandbox! TODO
      let change = (aValue != (checkbox.getAttribute("checked") == "true"));
      if (aValue)
        checkbox.setAttribute("checked", "true");
      else
        checkbox.removeAttribute("checked");

      if (change) {
        let event = this._document.createEvent("Events");
        event.initEvent("CheckboxStateChange", true, true);
        checkbox.dispatchEvent(event);
      }
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
      const dtOptions = {
        timeStyle: "short",
        dateStyle: "long"
      };
      this.__dateFormatter = new Services.intl.DateTimeFormat(undefined, dtOptions);
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
      feedTitleLink.setAttribute("title", titleText);
      feedTitleText.style.marginRight = titleImageWidth + "px";

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
        if (enc.hasKey("typeDesc"))
          type_text = enc.get("typeDesc");

        if (type_text && type_text.length > 0)
          mozicon = "moz-icon://goat?size=16&contentType=" + enc.get("type");
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
        enclosureDiv.appendChild(this._document.createTextNode( " (" + type_text + ")"));

      else if (size_text)
        enclosureDiv.appendChild(this._document.createTextNode( " (" + size_text + ")"));

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
    let fph = Services.io.getProtocolHandler("file")
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
                                feedType: this._getFeedType() });
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

  // EventListener
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
        LOG("Change fired");
        if (event.target.selectedOptions[0].id == "chooseApplicationMenuItem") {
          this._chooseClientApp(() => {
            // Select the (per-prefs) selected handler if no application
            // was selected
            LOG("Selected handler after callback");
            this._setAlwaysUseLabel();
          });
        } else {
          this._setAlwaysUseLabel();
        }
        break;
    }
  },

  _getWebHandlerElementsForURL(aURL) {
    return this._handlersList.querySelectorAll('[webhandlerurl="' + aURL + '"]');
  },

  _setSelectedHandlerResponse(handler, url) {
    LOG(`Selecting handler response ${handler} ${url}`);
    switch (handler) {
      case "web": {
        if (this._handlersList) {
          let handlers =
            this._getWebHandlerElementsForURL(url);
          if (handlers.length == 0) {
            LOG(`Selected web handler isn't in the menulist ${url}`);
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

  _initSubscriptionUI(setupMessage) {
    if (!this._handlersList)
      return;
    LOG("UI init");

    let feedType = this._getFeedType();

    // change the background
    let header = this._document.getElementById("feedHeader");
    switch (feedType) {
      case Ci.nsIFeed.TYPE_VIDEO:
        header.className = "videoPodcastBackground";
        break;

      case Ci.nsIFeed.TYPE_AUDIO:
        header.className = "audioPodcastBackground";
        break;

      default:
        header.className = "feedBackground";
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

    this._handlersList.appendChild(this._selectedAppMenuItem);

    // Create the menuitem for the default reader, but don't show/populate it until
    // we get confirmation of what it is from the parent
    menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("id", "defaultHandlerMenuItem");
    menuItem.setAttribute("handlerType", "client");
    menuItem.style.display = "none";

    this._defaultHandlerMenuItem = menuItem;
    this._handlersList.appendChild(this._defaultHandlerMenuItem);

    // "Choose Application..." menuitem
    menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("id", "chooseApplicationMenuItem");
    menuItem.textContent = this._getString("chooseApplicationMenuItem");

    this._handlersList.appendChild(menuItem);

    // separator
    let chooseAppSep = liveBookmarksMenuItem.nextElementSibling.cloneNode(false);
    chooseAppSep.textContent = liveBookmarksMenuItem.nextElementSibling.textContent;
    this._handlersList.appendChild(chooseAppSep);

    for (let handler of setupMessage.handlers) {
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
      this._handlersList.appendChild(menuItem);
    }

    this._setSelectedHandlerResponse(setupMessage.reader.handler, setupMessage.reader.url);

    if (setupMessage.defaultMenuItem) {
      LOG(`Setting default menu item ${setupMessage.defaultMenuItem}`);
      this._setApplicationLauncherMenuItem(this._defaultHandlerMenuItem, setupMessage.defaultMenuItem);
    }
    if (setupMessage.selectedMenuItem) {
      LOG(`Setting selected menu item ${setupMessage.selectedMenuItem}`);
      this._setApplicationLauncherMenuItem(this._selectedAppMenuItem, setupMessage.selectedMenuItem);
    }

    // "Subscribe using..."
    this._setSubscribeUsingLabel();

    // "Always use..." checkbox initial state
    this._setCheckboxCheckedState(setupMessage.reader.alwaysUse);
    this._setAlwaysUseLabel();

    // We update the "Always use.." checkbox label whenever the selected item
    // in the list is changed
    this._handlersList.addEventListener("change", this);

    // Set up the "Subscribe Now" button
    this._document.getElementById("subscribeButton")
        .addEventListener("click", this);

    // first-run ui
    if (setupMessage.showFirstRunUI) {
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

      header.setAttribute("firstrun", "true");

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
    if (window != window.top && !gCanFrameFeeds) {
      return;
    }
    this._feedURI = this._getOriginalURI(window);
    if (!this._feedURI)
      return;

    this._window = window;
    this._document = window.document;
    this._handlersList = this._document.getElementById("handlersMenuList");

    this._feedPrincipal = Services.scriptSecurityManager.createCodebasePrincipal(this._feedURI, {});

    LOG("Subscribe Preview: feed uri = " + this._window.location.href);


    this._mm.addMessageListener("FeedWriter:PreferenceUpdated", this);
    this._mm.addMessageListener("FeedWriter:SetApplicationLauncherMenuItem", this);
    this._mm.addMessageListener("FeedWriter:GetSubscriptionUIResponse", this);
    this._mm.addMessageListener("FeedWriter:SetFeedPrefsAndSubscribeResponse", this);

    const feedType = this._getFeedType();
    this._mm.sendAsyncMessage("FeedWriter:GetSubscriptionUI",
                              { feedType });
  },

  receiveMessage(msg) {
    if (!this._window) {
      // this._window is null unless this.init was called with a trusted
      // window object.
      return;
    }
    LOG(`received message from parent ${msg.name}`);
    switch (msg.name) {
      case "FeedWriter:PreferenceUpdated":
        // This is called when browser-feeds.js spots a pref change
        // This will happen when
        // - about:preferences#general changes
        // - another feed reader page changes the preference
        // - when this page itself changes the select and there isn't a redirect
        //   bookmarks and launching an external app means the page stays open after subscribe
        const feedType = this._getFeedType();
        LOG(`Got prefChange! ${JSON.stringify(msg.data)} current type: ${feedType}`);
        let feedTypePref = msg.data.default;
        if (feedType in msg.data) {
          feedTypePref = msg.data[feedType];
        }
        LOG(`Got pref ${JSON.stringify(feedTypePref)}`);
        this._setCheckboxCheckedState(feedTypePref.alwaysUse);
        this._setSelectedHandlerResponse(feedTypePref.handler, feedTypePref.url);
        this._setAlwaysUseLabel();
        break;
      case "FeedWriter:SetFeedPrefsAndSubscribeResponse":
        LOG(`FeedWriter:SetFeedPrefsAndSubscribeResponse - Redirecting ${msg.data.redirect}`);
        this._window.location.href = msg.data.redirect;
        break;
      case "FeedWriter:GetSubscriptionUIResponse":
        // Set up the subscription UI
        this._initSubscriptionUI(msg.data);
        break;
      case "FeedWriter:SetApplicationLauncherMenuItem":
        LOG(`FeedWriter:SetApplicationLauncherMenuItem - picked ${msg.data.name}`);
        this._setApplicationLauncherMenuItem(this._selectedAppMenuItem, msg.data.name);
        // Potentially a bit racy, but I don't think we can get into a state where this callback is set and
        // we're not coming back from ChooseClientApp in browser-feeds.js
        if (this._subscribeCallback) {
          this._subscribeCallback();
          this._subscribeCallback = null;
        }
        break;
    }
  },

  _setApplicationLauncherMenuItem(menuItem, aName) {
    /* unselect all handlers */
    [...this._handlersList.children].forEach((option) => {
      option.removeAttribute("selected");
    });
    menuItem.textContent = aName;
    menuItem.style.display = "";
    menuItem.selected = true;
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
    if (!this._window) {
      return;
    }
    this._document.getElementById("subscribeButton")
        .removeEventListener("click", this);
    this._handlersList
        .removeEventListener("change", this);
    this._document = null;
    this._window = null;
    this._handlersList = null;

    this._removeFeedFromCache();
    this.__bundle = null;
    this._feedURI = null;

    this._selectedApp = undefined;
    this._selectedAppMenuItem = null;
    this._defaultHandlerMenuItem = null;
  },

  _removeFeedFromCache() {
    if (this._window && this._feedURI) {
      let feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                        getService(Ci.nsIFeedResultService);
      feedService.removeFeedResult(this._feedURI);
      this._feedURI = null;
    }
  },

  subscribe() {
    if (!this._window) {
      return;
    }
    let feedType = this._getFeedType();

    // Subscribe to the feed using the selected handler and save prefs
    let defaultHandler = "reader";
    let useAsDefault = this._document.getElementById("alwaysUse").getAttribute("checked");

    let selectedItem = this._handlersList.selectedOptions[0];
    let subscribeCallback = () => {
      let feedReader = null;
      let settings = {
        feedType,
        useAsDefault,
        // Pull the title and subtitle out of the document
        feedTitle: this._document.getElementById(TITLE_ID).textContent,
        feedSubtitle: this._document.getElementById(SUBTITLE_ID).textContent,
        feedLocation: this._window.location.href
      };
      if (selectedItem.hasAttribute("webhandlerurl")) {
        feedReader = "web";
        settings.uri = selectedItem.getAttribute("webhandlerurl");
      } else {
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
      }
      settings.reader = feedReader;

      // If "Always use..." is checked, we should set PREF_*SELECTED_ACTION
      // to either "reader" (If a web reader or if an application is selected),
      // or to "bookmarks" (if the live bookmarks option is selected).
      // Otherwise, we should set it to "ask"
      if (!useAsDefault) {
        defaultHandler = "ask";
      }
      settings.action = defaultHandler;
      LOG(`FeedWriter:SetFeedPrefsAndSubscribe - ${JSON.stringify(settings)}`);
      this._mm.sendAsyncMessage("FeedWriter:SetFeedPrefsAndSubscribe",
                                settings);
    };

    // Show the file picker before subscribing if the
    // choose application menuitem was chosen using the keyboard
    if (selectedItem.id == "chooseApplicationMenuItem") {
      this._chooseClientApp(aResult => {
        if (aResult) {
          selectedItem =
            this._handlersList.selectedOptions[0];
          subscribeCallback();
        }
      });
    } else {
      subscribeCallback();
    }
  },

  classID: FEEDWRITER_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FeedWriter]);
