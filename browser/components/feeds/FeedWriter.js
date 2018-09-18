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
  Services.telemetry.scalarAdd("browser.feeds.preview_loaded", 1);
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
        dateStyle: "long",
      };
      this.__dateFormatter = new Services.intl.DateTimeFormat(undefined, dtOptions);
    }
    return this.__dateFormatter;
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
   * Returns the original URI object of the feed and ensures that this
   * component is only ever invoked from the preview document.
   * @param aWindow
   *        The window of the document invoking the BrowserFeedWriter
   */
  _getOriginalURI(aWindow) {
    let docShell = aWindow.docShell;
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
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    }).URI;

    if (resolvedURI.equals(chan.URI))
      return chan.originalURI;

    return null;
  },

  _window: null,
  _document: null,
  _feedURI: null,
  _feedPrincipal: null,

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

    this._feedPrincipal = Services.scriptSecurityManager.createCodebasePrincipal(this._feedURI, {});

    LOG("Subscribe Preview: feed uri = " + this._window.location.href);
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
    this._document = null;
    this._window = null;

    this._removeFeedFromCache();
    this.__bundle = null;
    this._feedURI = null;
  },

  _removeFeedFromCache() {
    if (this._window && this._feedURI) {
      let feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                        getService(Ci.nsIFeedResultService);
      feedService.removeFeedResult(this._feedURI);
      this._feedURI = null;
    }
  },

  classID: FEEDWRITER_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsIDOMGlobalPropertyInitializer]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FeedWriter]);
