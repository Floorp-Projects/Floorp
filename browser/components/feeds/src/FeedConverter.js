# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Feed Stream Converter.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function LOG(str) {
  dump("*** " + str + "\n");
}

const FC_CLASSID = Components.ID("{229fa115-9412-4d32-baf3-2fc407f76fb1}");
const FC_CLASSNAME = "Feed Stream Converter";
const FS_CLASSID = Components.ID("{2376201c-bbc6-472f-9b62-7548040a61c6}");
const FS_CLASSNAME = "Feed Result Service";
const FS_CONTRACTID = "@mozilla.org/browser/feeds/result-service;1";
const FPH_CONTRACTID = "@mozilla.org/network/protocol;1?name=feed";
const FPH_CLASSID = Components.ID("{4f91ef2e-57ba-472e-ab7a-b4999e42d6c0}");
const FPH_CLASSNAME = "Feed Protocol Handler";
const PCPH_CONTRACTID = "@mozilla.org/network/protocol;1?name=pcast";
const PCPH_CLASSID = Components.ID("{1c31ed79-accd-4b94-b517-06e0c81999d5}");
const PCPH_CLASSNAME = "Podcast Protocol Handler";
const FHS_CONTRACTID = "@mozilla.org/browser/feeds/handler-service;1";
const FHS_CLASSID = Components.ID("{792a7e82-06a0-437c-af63-b2d12e808acc}");
const FHS_CLASSNAME = "Feed Handler Service";

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_ANY = "*/*";

const FEEDHANDLER_URI = "about:feeds";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_HANDLER = "browser.feeds.handler";
const PREF_SKIP_PREVIEW_PAGE = "browser.feeds.skip_preview_page";

function safeGetBoolPref(pref, defaultValue) {
  var prefs =   
      Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch);
  try {
    return prefs.getBoolPref(pref);
  }
  catch (e) {
  }
  return defaultValue;
}

function safeGetCharPref(pref, defaultValue) {
  var prefs =   
      Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch);
  try {
    return prefs.getCharPref(pref);
  }
  catch (e) {
  }
  return defaultValue;
}

function FeedConverter() {
}
FeedConverter.prototype = {
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
   * See nsIStreamConverter.idl
   */
  canConvert: function FC_canConvert(sourceType, destinationType) {
    // We only support one conversion.
    return destinationType == TYPE_ANY && sourceType == TYPE_MAYBE_FEED;
  },
  
  /**
   * See nsIStreamConverter.idl
   */
  convert: function FC_convert(sourceStream, sourceType, destinationType, 
                               context) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  /**
   * See nsIStreamConverter.idl
   */
  asyncConvertData: function FC_asyncConvertData(sourceType, destinationType,
                                                 listener, context) {
    this._listener = listener;
  },
  
  /**
   * Whether or not the preview page is being forced.
   */
  _forcePreviewPage: false,
  
  /**
   * See nsIFeedResultListener.idl
   */
  handleResult: function FC_handleResult(result) {
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
    // that determination make a judgement about type. 
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
    var feedService = 
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);
    if (!this._forcePreviewPage) {
      var skipPreview = safeGetBoolPref(PREF_SKIP_PREVIEW_PAGE, false);
      if (skipPreview) {
        var handler = safeGetCharPref(PREF_SELECTED_HANDLER, "bookmarks");
        if (handler == "web") {
          var wccr = 
              Cc["@mozilla.org/web-content-handler-registrar;1"].
              getService(Ci.nsIWebContentConverterService);
          var feed = result.doc.QueryInterface(Ci.nsIFeed);
          if (feed.type == Ci.nsIFeed.TYPE_FEED &&
              wccr.getAutoHandler(TYPE_MAYBE_FEED)) {
            wccr.loadPreferredHandler(this._request);
            return;
          }
        }
        else {
          feedService.addToClientReader(result.uri.spec);
          return;
        }
      }
    }
        
    // If there was no automatic handler, or this was a podcast, photostream or
    // some other kind of application, we must always show the preview page...
  
    // Store the result in the result service so that the display page can 
    // access it.
    feedService.addFeedResult(result);

    // Now load the actual XUL document.
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var chromeURI = ios.newURI(FEEDHANDLER_URI, null, null);
    var chromeChannel = ios.newChannelFromURI(chromeURI, null);
    chromeChannel.originalURI = result.uri;
    chromeChannel.asyncOpen(this._listener, null);
  },
  
  /**
   * See nsIStreamListener.idl
   */
  onDataAvailable: function FC_onDataAvailable(request, context, inputStream, 
                                               sourceOffset, count) {
    this._processor.onDataAvailable(request, context, inputStream,
                                    sourceOffset, count);
  },
  
  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest: function FC_onStartRequest(request, context) {
    var channel = request.QueryInterface(Ci.nsIChannel);
    this._request = request;
    
    // Save and reset the forced state bit early, in case there's some kind of
    // error.
    var feedService = 
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
  onStopRequest: function FC_onStopReqeust(request, context, status) {
    this._processor.onStopRequest(request, context, status);
  },
  
  /**
   * See nsISupports.idl
   */
  QueryInterface: function FC_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedResultListener) ||
        iid.equals(Ci.nsIStreamConverter) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver)||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
};

var FeedConverterFactory = {
  createInstance: function FS_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return new FeedConverter().QueryInterface(iid);
  },

  QueryInterface: function FS_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

/**
 * Keeps parsed FeedResults around for use elsewhere in the UI after the stream
 * converter completes. 
 */
var FeedResultService = {
  
  /**
   * A URI spec -> nsIFeedResult hash
   */
  _results: { },
  
  /**
   * See nsIFeedService.idl
   */
  forcePreviewPage: false,
  
  /**
   * See nsIFeedService.idl
   */
  addToClientReader: function FRS_addToClientReader(uri) {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var handler = safeGetCharPref(PREF_SELECTED_HANDLER, "bookmarks");
    switch (handler) {
    case "client":
      try {
        var clientApp = 
            prefs.getComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile);
      }
      catch (e) {
        return;
      }
      var process = 
          Cc["@mozilla.org/process/util;1"].
          createInstance(Ci.nsIProcess);
      process.init(clientApp);
      process.run(false, [uri], 1);
      break;
    case "bookmarks":
      var wm = 
          Cc["@mozilla.org/appshell/window-mediator;1"].
          getService(Ci.nsIWindowMediator);
      var topWindow = wm.getMostRecentWindow("navigator:browser");
#ifdef MOZ_PLACES
      topWindow.PlacesCommandHook.addLiveBookmark(uri);
#else
      topWindow.FeedHandler.addLiveBookmark(uri);
#endif
      break;
    }
  },
  
  /**
   * See nsIFeedService.idl
   */
  addFeedResult: function FRS_addFeedResult(feedResult) {
    NS_ASSERT(feedResult.uri != null, "null URI!");
    NS_ASSERT(feedResult.uri != null, "null feedResult!");
    this._results[feedResult.uri.spec] = feedResult;
  },
  
  /**
   * See nsIFeedService.idl
   */
  getFeedResult: function RFS_getFeedResult(uri) {
    NS_ASSERT(uri != null, "null URI!");
    return this._results[uri.spec];
  },
  
  /**
   * See nsIFeedService.idl
   */
  removeFeedResult: function FRS_removeFeedResult(uri) {
    NS_ASSERT(uri != null, "null URI!");
    if (uri.spec in this._results)
      delete this._results[uri.spec];
  },

  createInstance: function FRS_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  
  QueryInterface: function FRS_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedResultService) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

/**
 * A protocol handler that converts the URIs of Apple's various bogo protocol
 * schemes into http, as they should be. Mostly, this object just forwards 
 * things through to the HTTP protocol handler.
 */
function FeedProtocolHandler(scheme) {
  this._scheme = scheme;
  var ios = 
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
  this._http = ios.getProtocolHandler("http");
}
FeedProtocolHandler.prototype = {
  _scheme: "",
  get scheme() {
    return this._scheme;
  },
  
  get protocolFlags() {
    return this._http.protocolFlags;
  },
  
  get defaultPort() {
    return this._http.defaultPort;
  },
  
  allowPort: function FPH_allowPort(port, scheme) {
    return this._http.allowPort(port, scheme);
  },
  
  newURI: function FPH_newURI(spec, originalCharset, baseURI) {
    var uri = 
        Cc["@mozilla.org/network/standard-url;1"].
        createInstance(Ci.nsIStandardURL);
    uri.init(Ci.nsIStandardURL.URLTYPE_STANDARD, 80, spec, originalCharset,
             baseURI);
    return uri;
  },
  
  newChannel: function FPH_newChannel(uri) {
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    // feed: URIs either start feed://, in which case the real scheme is http:
    // or feed:http(s)://, (which by now we've changed to feed://realscheme//)
    const httpsChunk = "feed://https//";
    const httpChunk = "feed://http//";
    if (uri.spec.substr(0, httpsChunk.length) == httpsChunk)
      uri.spec = "https://" + uri.spec.substr(httpsChunk.length);
    else if (uri.spec.substr(0, httpChunk.length) == httpChunk)
      uri.spec = "http://" + uri.spec.substr(httpChunk.length);
    else
      uri.scheme = "http";

    var channel = ios.newChannelFromURI(uri, null);
    channel.originalURI = uri;
    return channel;
  },
  
  QueryInterface: function FPH_QueryInterface(iid) {
    if (iid.equals(Ci.nsIProtocolHandler) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }  
};

var Module = {
  QueryInterface: function M_QueryInterface(iid) {
    if (iid.equals(Ci.nsIModule) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  getClassObject: function M_getClassObject(cm, cid, iid) {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    
    if (cid.equals(FS_CLASSID))
      return FeedResultService;
    if (cid.equals(FPH_CLASSID))
      return new GenericComponentFactory(FeedProtocolHandler, "feed");
    if (cid.equals(PCPH_CLASSID))
      return new GenericComponentFactory(FeedProtocolHandler, "pcast");
    if (cid.equals(FC_CLASSID))
      return new GenericComponentFactory(FeedConverter);
      
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  registerSelf: function M_registerSelf(cm, file, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    
    cr.registerFactoryLocation(FS_CLASSID, FS_CLASSNAME, FS_CONTRACTID,
                               file, location, type);
    cr.registerFactoryLocation(FPH_CLASSID, FPH_CLASSNAME, FPH_CONTRACTID,
                               file, location, type);
    cr.registerFactoryLocation(PCPH_CLASSID, PCPH_CLASSNAME, PCPH_CONTRACTID,
                               file, location, type);

    // The feed converter is always attached, since parsing must be done to 
    // determine whether or not auto-handling can occur. 
    const converterPrefix = "@mozilla.org/streamconv;1?from=";
    var converterContractID = 
        converterPrefix + TYPE_MAYBE_FEED + "&to=" + TYPE_ANY;
    cr.registerFactoryLocation(FC_CLASSID, FC_CLASSNAME, converterContractID,
                               file, location, type);
  },
  
  unregisterSelf: function M_unregisterSelf(cm, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    cr.unregisterFactoryLocation(FPH_CLASSID, location);
    cr.unregisterFactoryLocation(PCPH_CLASSID, location);
  },
  
  canUnload: function M_canUnload(cm) {
    return true;
  }
};

function NSGetModule(cm, file) {
  return Module;
}

#include ../../../../toolkit/content/debug.js
#include GenericFactory.js
