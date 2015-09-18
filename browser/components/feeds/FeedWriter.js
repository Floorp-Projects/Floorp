# -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const FEEDWRITER_CID = Components.ID("{49bb6593-3aff-4eb3-a068-2712c28bd58e}");
const FEEDWRITER_CONTRACTID = "@mozilla.org/browser/feeds/result-writer;1";

function LOG(str) {
  var prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);

  var shouldLog = false;
  try {
    shouldLog = prefB.getBoolPref("feeds.log");
  } 
  catch (ex) {
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
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  try {
    return ios.newURI(aURLSpec, aCharset, null);
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
  var units = ["bytes", "kilobyte", "megabyte", "gigabyte"];
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
  this._defaultHandlerMenuItem = null;
}

FeedWriter.prototype = {
  _mimeSvc      : Cc["@mozilla.org/mime;1"].
                  getService(Ci.nsIMIMEService),

  _getPropertyAsBag: function FW__getPropertyAsBag(container, property) {
    return container.fields.getProperty(property).
                     QueryInterface(Ci.nsIPropertyBag2);
  },

  _getPropertyAsString: function FW__getPropertyAsString(container, property) {
    try {
      return container.fields.getPropertyAsAString(property);
    }
    catch (e) {
    }
    return "";
  },

  _setContentText: function FW__setContentText(id, text) {
    var element = this._document.getElementById(id);
    var textNode = text.createDocumentFragment(element);
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
  _safeSetURIAttribute:
  function FW__safeSetURIAttribute(element, attribute, uri) {
    var secman = Cc["@mozilla.org/scriptsecuritymanager;1"].
                 getService(Ci.nsIScriptSecurityManager);
    const flags = Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;
    try {
      // TODO Is this necessary?
      secman.checkLoadURIStrWithPrincipal(this._feedPrincipal, uri, flags);
      // checkLoadURIStrWithPrincipal will throw if the link URI should not be
      // loaded, either because our feedURI isn't allowed to load it or per
      // the rules specified in |flags|, so we'll never "linkify" the link...
    }
    catch (e) {
      // Not allowed to load this link because secman.checkLoadURIStr threw
      return;
    }

    element.setAttribute(attribute, uri);
  },

  __faviconService: null,
  get _faviconService() {
    if (!this.__faviconService)
      this.__faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                              getService(Ci.nsIFaviconService);

    return this.__faviconService;
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

  _getFormattedString: function FW__getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },
  
  _getString: function FW__getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  /* Magic helper methods to be used instead of xbl properties */
  _getSelectedItemFromMenulist: function FW__getSelectedItemFromList(aList) {
    var node = aList.firstChild.firstChild;
    while (node) {
      if (node.localName == "menuitem" && node.getAttribute("selected") == "true")
        return node;

      node = node.nextSibling;
    }

    return null;
  },

  _setCheckboxCheckedState: function FW__setCheckboxCheckedState(aCheckbox, aValue) {
    // see checkbox.xml, xbl bindings are not applied within the sandbox! TODO
    var change = (aValue != (aCheckbox.getAttribute('checked') == 'true'));
    if (aValue)
      aCheckbox.setAttribute('checked', 'true');
    else
      aCheckbox.removeAttribute('checked');

    if (change) {
      var event = this._document.createEvent('Events');
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
  _parseDate: function FW__parseDate(dateString) {
    // Convert the date into the user's local time zone
    var dateObj = new Date(dateString);

    // Make sure the date we're given is valid.
    if (!dateObj.getTime())
      return false;

    var dateService = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                      getService(Ci.nsIScriptableDateFormat);
    return dateService.FormatDateTime("", dateService.dateFormatLong, dateService.timeFormatNoSeconds,
                                      dateObj.getFullYear(), dateObj.getMonth()+1, dateObj.getDate(),
                                      dateObj.getHours(), dateObj.getMinutes(), dateObj.getSeconds());
  },

  /**
   * Returns the feed type.
   */
  __feedType: null,
  _getFeedType: function FW__getFeedType() {
    if (this.__feedType != null)
      return this.__feedType;

    try {
      // grab the feed because it's got the feed.type in it.
      var container = this._getContainer();
      var feed = container.QueryInterface(Ci.nsIFeed);
      this.__feedType = feed.type;
      return feed.type;
    } catch (ex) { }

    return Ci.nsIFeed.TYPE_FEED;
  },

  /**
   * Maps a feed type to a maybe-feed mimetype.
   */
  _getMimeTypeForFeedType: function FW__getMimeTypeForFeedType() {
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
  _setTitleText: function FW__setTitleText(container) {
    if (container.title) {
      var title = container.title.plainText();
      this._setContentText(TITLE_ID, container.title);
      this._document.title = title;
    }

    var feed = container.QueryInterface(Ci.nsIFeed);
    if (feed && feed.subtitle)
      this._setContentText(SUBTITLE_ID, container.subtitle);
  },

  /**
   * Writes the title image into the preview document if one is present.
   * @param   container
   *          The feed container
   */
  _setTitleImage: function FW__setTitleImage(container) {
    try {
      var parts = container.image;

      // Set up the title image (supplied by the feed)
      var feedTitleImage = this._document.getElementById("feedTitleImage");
      this._safeSetURIAttribute(feedTitleImage, "src", 
                                parts.getPropertyAsAString("url"));

      // Set up the title image link
      var feedTitleLink = this._document.getElementById("feedTitleLink");

      var titleText = this._getFormattedString("linkTitleTextFormat", 
                                               [parts.getPropertyAsAString("title")]);
      var feedTitleText = this._document.getElementById("feedTitleText");
      var titleImageWidth = parseInt(parts.getPropertyAsAString("width")) + 15;

      // Fix the margin on the main title, so that the image doesn't run over
      // the underline
      feedTitleLink.setAttribute('title', titleText);
      feedTitleText.style.marginRight = titleImageWidth + 'px';

      this._safeSetURIAttribute(feedTitleLink, "href",
                                parts.getPropertyAsAString("link"));
    }
    catch (e) {
      LOG("Failed to set Title Image (this is benign): " + e);
    }
  },

  /**
   * Writes all entries contained in the feed.
   * @param   container
   *          The container of entries in the feed
   */
  _writeFeedContent: function FW__writeFeedContent(container) {
    // Build the actual feed content
    var feed = container.QueryInterface(Ci.nsIFeed);
    if (feed.items.length == 0)
      return;

    var feedContent = this._document.getElementById("feedContent");

    for (var i = 0; i < feed.items.length; ++i) {
      var entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      entry.QueryInterface(Ci.nsIFeedContainer);

      var entryContainer = this._document.createElementNS(HTML_NS, "div");
      entryContainer.className = "entry";

      // If the entry has a title, make it a link
      if (entry.title) {
        var a = this._document.createElementNS(HTML_NS, "a");
        var span = this._document.createElementNS(HTML_NS, "span");
        a.appendChild(span);
        if (entry.title.base)
          span.setAttributeNS(XML_NS, "base", entry.title.base.spec);
        span.appendChild(entry.title.createDocumentFragment(a));

        // Entries are not required to have links, so entry.link can be null.
        if (entry.link)
          this._safeSetURIAttribute(a, "href", entry.link.spec);

        var title = this._document.createElementNS(HTML_NS, "h3");
        title.appendChild(a);

        var lastUpdated = this._parseDate(entry.updated);
        if (lastUpdated) {
          var dateDiv = this._document.createElementNS(HTML_NS, "div");
          dateDiv.className = "lastUpdated";
          dateDiv.textContent = lastUpdated;
          title.appendChild(dateDiv);
        }

        entryContainer.appendChild(title);
      }

      var body = this._document.createElementNS(HTML_NS, "div");
      var summary = entry.summary || entry.content;
      var docFragment = null;
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
          var a = this._document.createElementNS(HTML_NS, "a");
          a.appendChild(this._document.createTextNode("#"));
          this._safeSetURIAttribute(a, "href", entry.link.spec);
          body.appendChild(this._document.createTextNode(" "));
          body.appendChild(a);
        }

      }
      body.className = "feedEntryContent";
      entryContainer.appendChild(body);

      if (entry.enclosures && entry.enclosures.length > 0) {
        var enclosuresDiv = this._buildEnclosureDiv(entry);
        entryContainer.appendChild(enclosuresDiv);
      }

      var clearDiv = this._document.createElementNS(HTML_NS, "div");
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
  _getURLDisplayName: function FW__getURLDisplayName(aURL) {
    var url = makeURI(aURL);
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
  _buildEnclosureDiv: function FW__buildEnclosureDiv(entry) {
    var enclosuresDiv = this._document.createElementNS(HTML_NS, "div");
    enclosuresDiv.className = "enclosures";

    enclosuresDiv.appendChild(this._document.createTextNode(this._getString("mediaLabel")));

    var roundme = function(n) {
      return (Math.round(n * 100) / 100).toLocaleString();
    }

    for (var i_enc = 0; i_enc < entry.enclosures.length; ++i_enc) {
      var enc = entry.enclosures.queryElementAt(i_enc, Ci.nsIWritablePropertyBag2);

      if (!(enc.hasKey("url"))) 
        continue;

      var enclosureDiv = this._document.createElementNS(HTML_NS, "div");
      enclosureDiv.setAttribute("class", "enclosure");

      var mozicon = "moz-icon://.txt?size=16";
      var type_text = null;
      var size_text = null;

      if (enc.hasKey("type")) {
        type_text = enc.get("type");
        try {
          var handlerInfoWrapper = this._mimeSvc.getFromTypeAndExtension(enc.get("type"), null);

          if (handlerInfoWrapper)
            type_text = handlerInfoWrapper.description;

          if  (type_text && type_text.length > 0)
            mozicon = "moz-icon://goat?size=16&contentType=" + enc.get("type");

        } catch (ex) { }

      }

      if (enc.hasKey("length") && /^[0-9]+$/.test(enc.get("length"))) {
        var enc_size = convertByteUnits(parseInt(enc.get("length")));

        var size_text = this._getFormattedString("enclosureSizeText", 
                             [enc_size[0], this._getString(enc_size[1])]);
      }

      var iconimg = this._document.createElementNS(HTML_NS, "img");
      iconimg.setAttribute("src", mozicon);
      iconimg.setAttribute("class", "type-icon");
      enclosureDiv.appendChild(iconimg);

      enclosureDiv.appendChild(this._document.createTextNode( " " ));

      var enc_href = this._document.createElementNS(HTML_NS, "a");
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
  _getContainer: function FW__getContainer() {
    var feedService =
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);

    try {
      var result =
        feedService.getFeedResult(this._getOriginalURI(this._window));
    }
    catch (e) {
      LOG("Subscribe Preview: feed not available?!");
    }

    if (result.bozo) {
      LOG("Subscribe Preview: feed result is bozo?!");
    }

    try {
      var container = result.doc;
    }
    catch (e) {
      LOG("Subscribe Preview: no result.doc? Why didn't the original reload?");
      return null;
    }
    return container;
  },

  /**
   * Get the human-readable display name of a file. This could be the 
   * application name.
   * @param   file
   *          A nsIFile to look up the name of
   * @returns The display name of the application represented by the file.
   */
  _getFileDisplayName: function FW__getFileDisplayName(file) {
#ifdef XP_WIN
    if (file instanceof Ci.nsILocalFileWin) {
      try {
        return file.getVersionInfoField("FileDescription");
      } catch (e) {}
    }
#endif
#ifdef XP_MACOSX
    if (file instanceof Ci.nsILocalFileMac) {
      try {
        return file.bundleDisplayName;
      } catch (e) {}
    }
#endif
    return file.leafName;
  },

  /**
   * Get moz-icon url for a file
   * @param   file
   *          A nsIFile object for which the moz-icon:// is returned
   * @returns moz-icon url of the given file as a string
   */
  _getFileIconURL: function FW__getFileIconURL(file) {
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    var fph = ios.getProtocolHandler("file")
                 .QueryInterface(Ci.nsIFileProtocolHandler);
    var urlSpec = fph.getURLSpecFromFile(file);
    return "moz-icon://" + urlSpec + "?size=16";
  },

  /**
   * Helper method to set the selected application and system default
   * reader menuitems details from a file object
   *   @param aMenuItem
   *          The menuitem on which the attributes should be set
   *   @param aFile
   *          The menuitem's associated file
   */
  _initMenuItemWithFile: function(aMenuItem, aFile) {
    var label = this._getFileDisplayName(aFile);
    var image = this._getFileIconURL(aFile);
    aMenuitem.setAttribute('label', label);
    aMenuitem.setAttribute('image', image);
  },

  /**
   * Helper method to get an element in the XBL binding where the handler
   * selection UI lives
   */
  _getUIElement: function FW__getUIElement(id) {
    return this._document.getAnonymousElementByAttribute(
      this._document.getElementById("feedSubscribeLine"), "anonid", id);
  },

  /**
   * Displays a prompt from which the user may choose a (client) feed reader.
   * @param aCallback the callback method, passes in true if a feed reader was
   *        selected, false otherwise.
   */
  _chooseClientApp: function FW__chooseClientApp(aCallback) {
    try {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      let fpCallback = function fpCallback_done(aResult) {
        if (aResult == Ci.nsIFilePicker.returnOK) {
          this._selectedApp = fp.file;
          if (this._selectedApp) {
            // XXXben - we need to compare this with the running instance
            //          executable just don't know how to do that via script
            // XXXmano TBD: can probably add this to nsIShellService
#ifdef XP_WIN
#expand             if (fp.file.leafName != "__MOZ_APP_NAME__.exe") {
#else
#ifdef XP_MACOSX
#expand             if (fp.file.leafName != "__MOZ_MACBUNDLE_NAME__") {
#else
#expand             if (fp.file.leafName != "__MOZ_APP_NAME__-bin") {
#endif
#endif
              this._initMenuItemWithFile(this._selectedAppMenuItem,
                                         this._selectedApp);

              // Show and select the selected application menuitem
              this._selectedAppMenuItem.hidden = false;
              this._selectedAppMenuItem.doCommand();
              if (aCallback) {
                aCallback(true);
                return;
              }
            }
          }
        }
        if (aCallback) {
          aCallback(false);
        }
      }.bind(this);

      fp.init(this._window, this._getString("chooseApplicationDialogTitle"),
              Ci.nsIFilePicker.modeOpen);
      fp.appendFilters(Ci.nsIFilePicker.filterApps);
      fp.open(fpCallback);
    } catch(ex) {
    }
  },

  _setAlwaysUseCheckedState: function FW__setAlwaysUseCheckedState(feedType) {
    var checkbox = this._getUIElement("alwaysUse");
    if (checkbox) {
      var alwaysUse = false;
      try {
        var prefs = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);
        if (prefs.getCharPref(getPrefActionForType(feedType)) != "ask")
          alwaysUse = true;
      }
      catch(ex) { }
      this._setCheckboxCheckedState(checkbox, alwaysUse);
    }
  },

  _setSubscribeUsingLabel: function FW__setSubscribeUsingLabel() {
    var stringLabel = "subscribeFeedUsing";
    switch (this._getFeedType()) {
      case Ci.nsIFeed.TYPE_VIDEO:
        stringLabel = "subscribeVideoPodcastUsing";
        break;

      case Ci.nsIFeed.TYPE_AUDIO:
        stringLabel = "subscribeAudioPodcastUsing";
        break;
    }

    var subscribeUsing = this._getUIElement("subscribeUsingDescription");
    var label = this._getString(stringLabel);
    subscribeUsing.setAttribute('value', label);
  },

  _setAlwaysUseLabel: function FW__setAlwaysUseLabel() {
    var checkbox = this._getUIElement("alwaysUse");
    if (checkbox) {
      if (this._handlersMenuList) {
        var handlerName = this._getSelectedItemFromMenulist(this._handlersMenuList)
                              .getAttribute("label");
        var stringLabel = "alwaysUseForFeeds";
        switch (this._getFeedType()) {
          case Ci.nsIFeed.TYPE_VIDEO:
            stringLabel = "alwaysUseForVideoPodcasts";
            break;

          case Ci.nsIFeed.TYPE_AUDIO:
            stringLabel = "alwaysUseForAudioPodcasts";
            break;
        }

        var label = this._getFormattedString(stringLabel, [handlerName]);

        checkbox.setAttribute('label', label);
      }
    }
  },

  // nsIDomEventListener
  handleEvent: function(event) {
    if (event.target.ownerDocument != this._document) {
      LOG("FeedWriter.handleEvent: Someone passed the feed writer as a listener to the events of another document!");
      return;
    }

    if (event.type == "command") {
      switch (event.target.getAttribute("anonid")) {
        case "subscribeButton":
          this.subscribe();
          break;
        case "chooseApplicationMenuItem":
          /* Bug 351263: Make sure to not steal focus if the "Choose
           * Application" item is being selected with the keyboard. We do this
           * by ignoring command events while the dropdown is closed (user
           * arrowing through the combobox), but handling them while the
           * combobox dropdown is open (user pressed enter when an item was
           * selected). If we don't show the filepicker here, it will be shown
           * when clicking "Subscribe Now".
           */
          var popupbox = this._handlersMenuList.firstChild.boxObject;
          if (popupbox.popupState == "hiding") {
            this._chooseClientApp(function(aResult) {
              if (!aResult) {
                // Select the (per-prefs) selected handler if no application
                // was selected
                this._setSelectedHandler(this._getFeedType());
              }
            }.bind(this));
          }
          break;
        default:
          this._setAlwaysUseLabel();
      }
    }
  },

  _setSelectedHandler: function FW__setSelectedHandler(feedType) {
    var prefs =
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);

    var handler = "bookmarks";
    try {
      handler = prefs.getCharPref(getPrefReaderForType(feedType));
    }
    catch (ex) { }

    switch (handler) {
      case "web": {
        if (this._handlersMenuList) {
          var url;
          try {
            url = prefs.getComplexValue(getPrefWebForType(feedType), Ci.nsISupportsString).data;
          } catch (ex) {
            LOG("FeedWriter._setSelectedHandler: invalid or no handler in prefs");
            return;
          }
          var handlers =
            this._handlersMenuList.getElementsByAttribute("webhandlerurl", url);
          if (handlers.length == 0) {
            LOG("FeedWriter._setSelectedHandler: selected web handler isn't in the menulist")
            return;
          }

          handlers[0].doCommand();
        }
        break;
      }
      case "client": {
        try {
          this._selectedApp =
            prefs.getComplexValue(getPrefAppForType(feedType), Ci.nsILocalFile);
        }
        catch(ex) {
          this._selectedApp = null;
        }

        if (this._selectedApp) {
          this._initMenuItemWithFile(this._selectedAppMenuItem,
                                     this._selectedApp);
          this._selectedAppMenuItem.hidden = false;
          this._selectedAppMenuItem.doCommand();

          // Only show the default reader menuitem if the default reader
          // isn't the selected application
          if (this._defaultSystemReader) {
            var shouldHide =
              this._defaultSystemReader.path == this._selectedApp.path;
            this._defaultHandlerMenuItem.hidden = shouldHide;
          }
          break;
        }
      }
      case "bookmarks":
      default: {
        var liveBookmarksMenuItem = this._getUIElement("liveBookmarksMenuItem");
        if (liveBookmarksMenuItem)
          liveBookmarksMenuItem.doCommand();
      } 
    }
  },

  _initSubscriptionUI: function FW__initSubscriptionUI() {
    var handlersMenuPopup = this._getUIElement("handlersMenuPopup");
    if (!handlersMenuPopup)
      return;

    var feedType = this._getFeedType();

    // change the background
    var header = this._document.getElementById("feedHeader");
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

    var liveBookmarksMenuItem = this._getUIElement("liveBookmarksMenuItem");

    // Last-selected application
    var menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("anonid", "selectedAppMenuItem");
    menuItem.className = "menuitem-iconic selectedAppMenuItem";
    menuItem.setAttribute("handlerType", "client");
    try {
      var prefs = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefBranch);
      this._selectedApp = prefs.getComplexValue(getPrefAppForType(feedType),
                                                Ci.nsILocalFile);

      if (this._selectedApp.exists())
        this._initMenuItemWithFile(menuItem, this._selectedApp);
      else {
        // Hide the menuitem if the last selected application doesn't exist
        menuItem.setAttribute("hidden", true);
      }
    }
    catch(ex) {
      // Hide the menuitem until an application is selected
      menuItem.setAttribute("hidden", true);
    }
    this._selectedAppMenuItem = menuItem;

    handlersMenuPopup.appendChild(this._selectedAppMenuItem);

    // List the default feed reader
    try {
      this._defaultSystemReader = Cc["@mozilla.org/browser/shell-service;1"].
                                  getService(Ci.nsIShellService).
                                  defaultFeedReader;
      menuItem = liveBookmarksMenuItem.cloneNode(false);
      menuItem.removeAttribute("selected");
      menuItem.setAttribute("anonid", "defaultHandlerMenuItem");
      menuItem.className = "menuitem-iconic defaultHandlerMenuItem";
      menuItem.setAttribute("handlerType", "client");

      this._initMenuItemWithFile(menuItem, this._defaultSystemReader);

      // Hide the default reader item if it points to the same application
      // as the last-selected application
      if (this._selectedApp &&
          this._selectedApp.path == this._defaultSystemReader.path)
        menuItem.hidden = true;
    }
    catch(ex) { menuItem = null; /* no default reader */ }

    if (menuItem) {
      this._defaultHandlerMenuItem = menuItem;
      handlersMenuPopup.appendChild(this._defaultHandlerMenuItem);
    }

    // "Choose Application..." menuitem
    menuItem = liveBookmarksMenuItem.cloneNode(false);
    menuItem.removeAttribute("selected");
    menuItem.setAttribute("anonid", "chooseApplicationMenuItem");
    menuItem.className = "menuitem-iconic chooseApplicationMenuItem";
    menuItem.setAttribute("label", this._getString("chooseApplicationMenuItem"));

    handlersMenuPopup.appendChild(menuItem);

    // separator
    var chooseAppSep = liveBookmarksMenuItem.nextSibling.cloneNode(false);
    handlersMenuPopup.appendChild(chooseAppSep);

    // List of web handlers
    var wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
               getService(Ci.nsIWebContentConverterService);
    var handlers = wccr.getContentHandlers(this._getMimeTypeForFeedType(feedType));
    if (handlers.length != 0) {
      for (var i = 0; i < handlers.length; ++i) {
        if (!handlers[i].uri) {
          LOG("Handler with name " + handlers[i].name + " has no URI!? Skipping...");
          continue;
        }
        menuItem = liveBookmarksMenuItem.cloneNode(false);
        menuItem.removeAttribute("selected");
        menuItem.className = "menuitem-iconic";
        menuItem.setAttribute("label", handlers[i].name);
        menuItem.setAttribute("handlerType", "web");
        menuItem.setAttribute("webhandlerurl", handlers[i].uri);
        handlersMenuPopup.appendChild(menuItem);

        this._setFaviconForWebReader(handlers[i].uri, menuItem);
      }
    }

    this._setSelectedHandler(feedType);

    // "Subscribe using..."
    this._setSubscribeUsingLabel();

    // "Always use..." checkbox initial state
    this._setAlwaysUseCheckedState(feedType);
    this._setAlwaysUseLabel();

    // We update the "Always use.." checkbox label whenever the selected item
    // in the list is changed
    handlersMenuPopup.addEventListener("command", this, false);

    // Set up the "Subscribe Now" button
    this._getUIElement("subscribeButton")
        .addEventListener("command", this, false);

    // first-run ui
    var showFirstRunUI = true;
    try {
      showFirstRunUI = prefs.getBoolPref(PREF_SHOW_FIRST_RUN_UI);
    }
    catch (ex) { }
    if (showFirstRunUI) {
      var textfeedinfo1, textfeedinfo2;
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

      var feedinfo1 = this._document.getElementById("feedSubscriptionInfo1");
      var feedinfo1Str = this._getString(textfeedinfo1);
      var feedinfo2 = this._document.getElementById("feedSubscriptionInfo2");
      var feedinfo2Str = this._getString(textfeedinfo2);

      feedinfo1.textContent = feedinfo1Str;
      feedinfo2.textContent = feedinfo2Str;

      header.setAttribute('firstrun', 'true');

      prefs.setBoolPref(PREF_SHOW_FIRST_RUN_UI, false);
    }
  },

  /**
   * Returns the original URI object of the feed and ensures that this
   * component is only ever invoked from the preview document.  
   * @param aWindow 
   *        The window of the document invoking the BrowserFeedWriter
   */
  _getOriginalURI: function FW__getOriginalURI(aWindow) {
    var chan = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
               getInterface(Ci.nsIWebNavigation).
               QueryInterface(Ci.nsIDocShell).currentDocumentChannel;

    var nullPrincipal = Cc["@mozilla.org/nullprincipal;1"].
                        createInstance(Ci.nsIPrincipal);

    var resolvedURI = Cc["@mozilla.org/network/io-service;1"].
                      getService(Ci.nsIIOService).
                      newChannel2("about:feeds",
                                  null,
                                  null,
                                  null, // aLoadingNode
                                  nullPrincipal,
                                  null, // aTriggeringPrincipal
                                  Ci.nsILoadInfo.SEC_NORMAL,
                                  Ci.nsIContentPolicy.TYPE_OTHER).URI;

    if (resolvedURI.equals(chan.URI))
      return chan.originalURI;

    return null;
  },

  _window: null,
  _document: null,
  _feedURI: null,
  _feedPrincipal: null,
  _handlersMenuList: null,

  // BrowserFeedWriter WebIDL methods
  init: function FW_init(aWindow) {
    var window = aWindow;
    this._feedURI = this._getOriginalURI(window);
    if (!this._feedURI)
      return;

    this._window = window;
    this._document = window.document;
    this._document.getElementById("feedSubscribeLine").offsetTop;
    this._handlersMenuList = this._getUIElement("handlersMenuList");

    var secman = Cc["@mozilla.org/scriptsecuritymanager;1"].
                 getService(Ci.nsIScriptSecurityManager);
    this._feedPrincipal = secman.getSimpleCodebasePrincipal(this._feedURI);

    LOG("Subscribe Preview: feed uri = " + this._window.location.href);

    // Set up the subscription UI
    this._initSubscriptionUI();
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
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
  },

  writeContent: function FW_writeContent() {
    if (!this._window)
      return;

    try {
      // Set up the feed content
      var container = this._getContainer();
      if (!container)
        return;

      this._setTitleText(container);
      this._setTitleImage(container);
      this._writeFeedContent(container);
    }
    finally {
      this._removeFeedFromCache();
    }
  },

  close: function FW_close() {
    this._getUIElement("handlersMenuPopup")
        .removeEventListener("command", this, false);
    this._getUIElement("subscribeButton")
        .removeEventListener("command", this, false);
    this._document = null;
    this._window = null;
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
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
    this.__faviconService = null;
    this.__bundle = null;
    this._feedURI = null;

    this._selectedApp = undefined;
    this._selectedAppMenuItem = null;
    this._defaultHandlerMenuItem = null;
  },

  _removeFeedFromCache: function FW__removeFeedFromCache() {
    if (this._feedURI) {
      var feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                        getService(Ci.nsIFeedResultService);
      feedService.removeFeedResult(this._feedURI);
      this._feedURI = null;
    }
  },

  subscribe: function FW_subscribe() {
    var feedType = this._getFeedType();

    // Subscribe to the feed using the selected handler and save prefs
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var defaultHandler = "reader";
    var useAsDefault = this._getUIElement("alwaysUse").getAttribute("checked");

    var selectedItem = this._getSelectedItemFromMenulist(this._handlersMenuList);
    let subscribeCallback = function() {
      if (selectedItem.hasAttribute("webhandlerurl")) {
        var webURI = selectedItem.getAttribute("webhandlerurl");
        prefs.setCharPref(getPrefReaderForType(feedType), "web");

        var supportsString = Cc["@mozilla.org/supports-string;1"].
                             createInstance(Ci.nsISupportsString);
        supportsString.data = webURI;
        prefs.setComplexValue(getPrefWebForType(feedType), Ci.nsISupportsString,
                              supportsString);

        var wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                   getService(Ci.nsIWebContentConverterService);
        var handler = wccr.getWebContentHandlerByURI(this._getMimeTypeForFeedType(feedType), webURI);
        if (handler) {
          if (useAsDefault) {
            wccr.setAutoHandler(this._getMimeTypeForFeedType(feedType), handler);
          }

          this._window.location.href = handler.getHandlerURI(this._window.location.href);
        }
      } else {
        switch (selectedItem.getAttribute("anonid")) {
          case "selectedAppMenuItem":
            prefs.setComplexValue(getPrefAppForType(feedType), Ci.nsILocalFile, 
                                  this._selectedApp);
            prefs.setCharPref(getPrefReaderForType(feedType), "client");
            break;
          case "defaultHandlerMenuItem":
            prefs.setComplexValue(getPrefAppForType(feedType), Ci.nsILocalFile, 
                                  this._defaultSystemReader);
            prefs.setCharPref(getPrefReaderForType(feedType), "client");
            break;
          case "liveBookmarksMenuItem":
            defaultHandler = "bookmarks";
            prefs.setCharPref(getPrefReaderForType(feedType), "bookmarks");
            break;
        }
        var feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                          getService(Ci.nsIFeedResultService);

        // Pull the title and subtitle out of the document
        var feedTitle = this._document.getElementById(TITLE_ID).textContent;
        var feedSubtitle = this._document.getElementById(SUBTITLE_ID).textContent;
        feedService.addToClientReader(this._window.location.href, feedTitle, feedSubtitle, feedType);
      }

      // If "Always use..." is checked, we should set PREF_*SELECTED_ACTION
      // to either "reader" (If a web reader or if an application is selected),
      // or to "bookmarks" (if the live bookmarks option is selected).
      // Otherwise, we should set it to "ask"
      if (useAsDefault) {
        prefs.setCharPref(getPrefActionForType(feedType), defaultHandler);
      } else {
        prefs.setCharPref(getPrefActionForType(feedType), "ask");
      }
    }.bind(this);

    // Show the file picker before subscribing if the
    // choose application menuitem was chosen using the keyboard
    if (selectedItem.getAttribute("anonid") == "chooseApplicationMenuItem") {
      this._chooseClientApp(function(aResult) {
        if (aResult) {
          selectedItem =
            this._getSelectedItemFromMenulist(this._handlersMenuList);
          subscribeCallback();
        }
      }.bind(this));
    } else {
      subscribeCallback();
    }
  },

  // nsIObserver
  observe: function FW_observe(subject, topic, data) {
    if (!this._window) {
      // this._window is null unless this.init was called with a trusted
      // window object.
      return;
    }

    var feedType = this._getFeedType();

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

  /**
   * Sets the icon for the given web-reader item in the readers menu.
   * The icon is fetched and stored through the favicon service.
   *
   * @param aReaderUrl
   *        the reader url.
   * @param aMenuItem
   *        the reader item in the readers menulist.
   *
   * @note For privacy reasons we cannot set the image attribute directly
   *       to the icon url.  See Bug 358878 for details.
   */
  _setFaviconForWebReader:
  function FW__setFaviconForWebReader(aReaderUrl, aMenuItem) {
    var readerURI = makeURI(aReaderUrl);
    if (!/^https?$/.test(readerURI.scheme)) {
      // Don't try to get a favicon for non http(s) URIs.
      return;
    }
    var faviconURI = makeURI(readerURI.prePath + "/favicon.ico");
    var self = this;
    var usePrivateBrowsing = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsIWebNavigation)
                                         .QueryInterface(Ci.nsIDocShell)
                                         .QueryInterface(Ci.nsILoadContext)
                                         .usePrivateBrowsing;
    this._faviconService.setAndFetchFaviconForPage(readerURI, faviconURI, false,
      usePrivateBrowsing ? this._faviconService.FAVICON_LOAD_PRIVATE
                         : this._faviconService.FAVICON_LOAD_NON_PRIVATE,
      function (aURI, aDataLen, aData, aMimeType) {
        if (aDataLen > 0) {
          var dataURL = "data:" + aMimeType + ";base64," +
                        btoa(String.fromCharCode.apply(null, aData));
          aMenuItem.setAttribute('image', dataURL);
        }
      });
  },

  classID: FEEDWRITER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMEventListener, Ci.nsIObserver,
                                         Ci.nsINavHistoryObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FeedWriter]);
