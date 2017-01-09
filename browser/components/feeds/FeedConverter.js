/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/debug.js");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function LOG(str) {
  dump("*** " + str + "\n");
}

const FS_CONTRACTID = "@mozilla.org/browser/feeds/result-service;1";
const FPH_CONTRACTID = "@mozilla.org/network/protocol;1?name=feed";
const PCPH_CONTRACTID = "@mozilla.org/network/protocol;1?name=pcast";

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_MAYBE_VIDEO_FEED = "application/vnd.mozilla.maybe.video.feed";
const TYPE_MAYBE_AUDIO_FEED = "application/vnd.mozilla.maybe.audio.feed";
const TYPE_ANY = "*/*";

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

function safeGetCharPref(pref, defaultValue) {
  var prefs =
      Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch);
  try {
    return prefs.getCharPref(pref);
  } catch (e) {
  }
  return defaultValue;
}

function FeedConverter() {
}
FeedConverter.prototype = {
  classID: Components.ID("{229fa115-9412-4d32-baf3-2fc407f76fb1}"),

  /**
   * This is the downloaded text data for the feed.
   */
  _data: null,

  /**
   * This is the object listening to the conversion, which is ultimately the
   * docshell for the load.
   */
  _listener: null,

  /**
   * Records if the feed was sniffed
   */
  _sniffed: false,

  /**
   * See nsIStreamConverter.idl
   */
  convert(sourceStream, sourceType, destinationType,
          context) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * See nsIStreamConverter.idl
   */
  asyncConvertData(sourceType, destinationType,
                   listener, context) {
    this._listener = listener;
  },

  /**
   * Whether or not the preview page is being forced.
   */
  _forcePreviewPage: false,

  /**
   * Release our references to various things once we're done using them.
   */
  _releaseHandles() {
    this._listener = null;
    this._request = null;
    this._processor = null;
  },

  /**
   * See nsIFeedResultListener.idl
   */
  handleResult(result) {
    // Feeds come in various content types, which our feed sniffer coerces to
    // the maybe.feed type. However, feeds are used as a transport for
    // different data types, e.g. news/blogs (traditional feed), video/audio
    // (podcasts) and photos (photocasts, photostreams). Each of these is
    // different in that there's a different class of application suitable for
    // handling feeds of that type, but without a content-type differentiation
    // it is difficult for us to disambiguate.
    //
    // The other problem is that if the user specifies an auto-action handler
    // for one feed application, the fact that the content type is shared means
    // that all other applications will auto-load with that handler too,
    // regardless of the content-type.
    //
    // This means that content-type alone is not enough to determine whether
    // or not a feed should be auto-handled. This means that for feeds we need
    // to always use this stream converter, even when an auto-action is
    // specified, not the basic one provided by WebContentConverter. This
    // converter needs to consume all of the data and parse it, and based on
    // that determination make a judgment about type.
    //
    // Since there are no content types for this content, and I'm not going to
    // invent any, the upshot is that while a user can set an auto-handler for
    // generic feed content, the system will prevent them from setting an auto-
    // handler for other stream types. In those cases, the user will always see
    // the preview page and have to select a handler. We can guess and show
    // a client handler, but will not be able to show web handlers for those
    // types.
    //
    // If this is just a feed, not some kind of specialized application, then
    // auto-handlers can be set and we should obey them.
    try {
      let feedService =
          Cc["@mozilla.org/browser/feeds/result-service;1"].
          getService(Ci.nsIFeedResultService);
      if (!this._forcePreviewPage && result.doc) {
        let feed = result.doc.QueryInterface(Ci.nsIFeed);
        let handler = safeGetCharPref(getPrefActionForType(feed.type), "ask");

        if (handler != "ask") {
          if (handler == "reader")
            handler = safeGetCharPref(getPrefReaderForType(feed.type), "bookmarks");
          switch (handler) {
            case "web":
              let wccr =
                  Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                  getService(Ci.nsIWebContentConverterService);
              if ((feed.type == Ci.nsIFeed.TYPE_FEED &&
                   wccr.getAutoHandler(TYPE_MAYBE_FEED)) ||
                  (feed.type == Ci.nsIFeed.TYPE_VIDEO &&
                   wccr.getAutoHandler(TYPE_MAYBE_VIDEO_FEED)) ||
                  (feed.type == Ci.nsIFeed.TYPE_AUDIO &&
                   wccr.getAutoHandler(TYPE_MAYBE_AUDIO_FEED))) {
                wccr.loadPreferredHandler(this._request);
                return;
              }
              break;

            default:
              LOG("unexpected handler: " + handler);
              // fall through -- let feed service handle error
            case "bookmarks":
            case "client":
            case "default":
              try {
                let title = feed.title ? feed.title.plainText() : "";
                let desc = feed.subtitle ? feed.subtitle.plainText() : "";
                let feedReader = safeGetCharPref(getPrefActionForType(feedType), "bookmarks");
                feedService.addToClientReader(result.uri.spec, title, desc, feed.type, feedReader);
                return;
              } catch (ex) { /* fallback to preview mode */ }
          }
        }
      }

      let ios =
          Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);
      let chromeChannel;

      // handling a redirect, hence forwarding the loadInfo from the old channel
      // to the newchannel.
      let oldChannel = this._request.QueryInterface(Ci.nsIChannel);
      let loadInfo = oldChannel.loadInfo;

      // If there was no automatic handler, or this was a podcast,
      // photostream or some other kind of application, show the preview page
      // if the parser returned a document.
      if (result.doc) {

        // Store the result in the result service so that the display
        // page can access it.
        feedService.addFeedResult(result);

        // Now load the actual XUL document.
        let aboutFeedsURI = ios.newURI("about:feeds");
        chromeChannel = ios.newChannelFromURIWithLoadInfo(aboutFeedsURI, loadInfo);
        chromeChannel.originalURI = result.uri;

        // carry the origin attributes from the channel that loaded the feed.
        chromeChannel.owner =
          Services.scriptSecurityManager.createCodebasePrincipal(aboutFeedsURI,
                                                                 loadInfo.originAttributes);
      } else {
        chromeChannel = ios.newChannelFromURIWithLoadInfo(result.uri, loadInfo);
      }

      chromeChannel.loadGroup = this._request.loadGroup;
      chromeChannel.asyncOpen2(this._listener);
    } finally {
      this._releaseHandles();
    }
  },

  /**
   * See nsIStreamListener.idl
   */
  onDataAvailable(request, context, inputStream,
                  sourceOffset, count) {
    if (this._processor)
      this._processor.onDataAvailable(request, context, inputStream,
                                      sourceOffset, count);
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest(request, context) {
    let channel = request.QueryInterface(Ci.nsIChannel);

    // Check for a header that tells us there was no sniffing
    // The value doesn't matter.
    try {
      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
      // Make sure to check requestSucceeded before the potentially-throwing
      // getResponseHeader.
      if (!httpChannel.requestSucceeded) {
        // Just give up, but don't forget to cancel the channel first!
        request.cancel(Cr.NS_BINDING_ABORTED);
        return;
      }

      // Note: this throws if the header is not set.
      httpChannel.getResponseHeader("X-Moz-Is-Feed");
    } catch (ex) {
      this._sniffed = true;
    }

    this._request = request;

    // Save and reset the forced state bit early, in case there's some kind of
    // error.
    let feedService =
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);
    this._forcePreviewPage = feedService.forcePreviewPage;
    feedService.forcePreviewPage = false;

    // Parse feed data as it comes in
    this._processor =
        Cc["@mozilla.org/feed-processor;1"].
        createInstance(Ci.nsIFeedProcessor);
    this._processor.listener = this;
    this._processor.parseAsync(null, channel.URI);

    this._processor.onStartRequest(request, context);
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest(request, context, status) {
    if (this._processor)
      this._processor.onStopRequest(request, context, status);
  },

  /**
   * See nsISupports.idl
   */
  QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedResultListener) ||
        iid.equals(Ci.nsIStreamConverter) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
};

/**
 * Keeps parsed FeedResults around for use elsewhere in the UI after the stream
 * converter completes.
 */
function FeedResultService() {
}

FeedResultService.prototype = {
  classID: Components.ID("{2376201c-bbc6-472f-9b62-7548040a61c6}"),

  /**
   * A URI spec -> [nsIFeedResult] hash. We have to keep a list as the
   * value in case the same URI is requested concurrently.
   */
  _results: { },

  /**
   * See nsIFeedResultService.idl
   */
  forcePreviewPage: false,

  /**
   * See nsIFeedResultService.idl
   */
  addToClientReader(spec, title, subtitle, feedType, feedReader) {
    if (!feedReader) {
      feedReader = "default";
    }

    let handler = safeGetCharPref(getPrefActionForType(feedType), "bookmarks");
    if (handler == "ask" || handler == "reader")
      handler = feedReader;

    switch (handler) {
    case "client":
      Services.cpmm.sendAsyncMessage("FeedConverter:ExecuteClientApp",
                                     { spec,
                                       title,
                                       subtitle,
                                       feedHandler: getPrefAppForType(feedType) });
      break;
    case "default":
      // Default system feed reader
      Services.cpmm.sendAsyncMessage("FeedConverter:ExecuteClientApp",
                                     { spec,
                                       title,
                                       subtitle,
                                       feedHandler: "default" });
      break;
    default:
      // "web" should have been handled elsewhere
      LOG("unexpected handler: " + handler);
      // fall through
    case "bookmarks":
      Services.cpmm.sendAsyncMessage("FeedConverter:addLiveBookmark",
                                     { spec, title, subtitle });
      break;
    }
  },

  /**
   * See nsIFeedResultService.idl
   */
  addFeedResult(feedResult) {
    NS_ASSERT(feedResult.uri != null, "null URI!");
    NS_ASSERT(feedResult.uri != null, "null feedResult!");
    let spec = feedResult.uri.spec;
    if (!this._results[spec])
      this._results[spec] = [];
    this._results[spec].push(feedResult);
  },

  /**
   * See nsIFeedResultService.idl
   */
  getFeedResult(uri) {
    NS_ASSERT(uri != null, "null URI!");
    let resultList = this._results[uri.spec];
    for (let result of resultList) {
      if (result.uri == uri)
        return result;
    }
    return null;
  },

  /**
   * See nsIFeedResultService.idl
   */
  removeFeedResult(uri) {
    NS_ASSERT(uri != null, "null URI!");
    let resultList = this._results[uri.spec];
    if (!resultList)
      return;
    let deletions = 0;
    for (let i = 0; i < resultList.length; ++i) {
      if (resultList[i].uri == uri) {
        delete resultList[i];
        ++deletions;
      }
    }

    // send the holes to the end
    resultList.sort();
    // and trim the list
    resultList.splice(resultList.length - deletions, deletions);
    if (resultList.length == 0)
      delete this._results[uri.spec];
  },

  createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedResultService) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

/**
 * A protocol handler that attempts to deal with the variant forms of feed:
 * URIs that are actually either http or https.
 */
function GenericProtocolHandler() {
}
GenericProtocolHandler.prototype = {
  _init(scheme) {
    let ios =
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
    this._http = ios.getProtocolHandler("http");
    this._scheme = scheme;
  },

  get scheme() {
    return this._scheme;
  },

  get protocolFlags() {
    let {URI_DANGEROUS_TO_LOAD, ALLOWS_PROXY_HTTP, ALLOWS_PROXY} =
      Ci.nsIProtocolHandler;
    return URI_DANGEROUS_TO_LOAD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP;
  },

  get defaultPort() {
    return this._http.defaultPort;
  },

  allowPort(port, scheme) {
    return this._http.allowPort(port, scheme);
  },

  newURI(spec, originalCharset, baseURI) {
    // Feed URIs can be either nested URIs of the form feed:realURI (in which
    // case we create a nested URI for the realURI) or feed://example.com, in
    // which case we create a nested URI for the real protocol which is http.

    let scheme = this._scheme + ":";
    if (spec.substr(0, scheme.length) != scheme)
      throw Cr.NS_ERROR_MALFORMED_URI;

    let prefix = spec.substr(scheme.length, 2) == "//" ? "http:" : "";
    let inner = Services.io.newURI(spec.replace(scheme, prefix),
                                   originalCharset, baseURI);

    if (!["http", "https"].includes(inner.scheme))
      throw Cr.NS_ERROR_MALFORMED_URI;

    let uri = Services.io.QueryInterface(Ci.nsINetUtil).newSimpleNestedURI(inner);
    uri.spec = inner.spec.replace(prefix, scheme);
    return uri;
  },

  newChannel2(aUri, aLoadInfo) {
    let inner = aUri.QueryInterface(Ci.nsINestedURI).innerURI;
    let channel = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService).
                  newChannelFromURIWithLoadInfo(inner, aLoadInfo);

    if (channel instanceof Components.interfaces.nsIHttpChannel)
      // Set this so we know this is supposed to be a feed
      channel.setRequestHeader("X-Moz-Is-Feed", "1", false);
    channel.originalURI = aUri;
    return channel;
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIProtocolHandler) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function FeedProtocolHandler() {
  this._init('feed');
}
FeedProtocolHandler.prototype = new GenericProtocolHandler();
FeedProtocolHandler.prototype.classID = Components.ID("{4f91ef2e-57ba-472e-ab7a-b4999e42d6c0}");

function PodCastProtocolHandler() {
  this._init('pcast');
}
PodCastProtocolHandler.prototype = new GenericProtocolHandler();
PodCastProtocolHandler.prototype.classID = Components.ID("{1c31ed79-accd-4b94-b517-06e0c81999d5}");

var components = [FeedConverter,
                  FeedResultService,
                  FeedProtocolHandler,
                  PodCastProtocolHandler];


this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
