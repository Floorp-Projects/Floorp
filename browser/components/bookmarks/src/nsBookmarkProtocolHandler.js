/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

// A bookmark URI can look like this:
// moz-bookmark://http%3A//www.bengoodger.com/?keyword=bg&name=Millennium&description=Ben's%20Site&charset=ISO-8859-1
function nsBookmark(aName, aURL, aKeyword, aDescription, aCharset, aWebPanel)
{
  this.name = aName || null;
  this.url = aURL || null;
  this.keyword = aKeyword || null;
  this.description = aDescription || null;
  this.charset = aCharset || null;
  this.webpanel = aWebPanel || null;
}

var nsBookmarkProtocolUtils = {
  parseBookmarkURL: function (aURL) {
    var re = /moz-bookmark:\/\/(\S*)\?(\S*)/i;
    var rv = aURL.match(re);
    
    var bookmark = new nsBookmark();
    
    bookmark.url = decodeURI(rv[1]);
    if (bookmark.url == "") 
      return null;
      
    var propertyPairs = rv[2].split("&");
    for (var i = 0; i < propertyPairs.length; ++i) {
      var propertyData = propertyPairs[i].split("=");
      bookmark[propertyData[0]] = decodeURIComponent(propertyData[1]);
    }
    
    return bookmark;
  }
};

function nsBookmarkProtocolHandler()
{
}

nsBookmarkProtocolHandler.prototype = {
  _PHIID: Components.interfaces.nsIProtocolHandler,
  
  get scheme()          { return "moz-bookmark"; },
  get protocolFlags()   { return this._PHIID.URI_NORELATIVE | this._PHIID.URI_NOAUTH },
  get defaultPort()     { return 0; },
  
  allowPort: function (aPort, aScheme)
  {
    return false;
  },
  
  newURI: function (aSpec, aOriginalCharset, aBaseURI)
  {
    const nsIStandardURL = Components.interfaces.nsIStandardURL;
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(nsIStandardURL);
    uri.init(nsIStandardURL.URLTYPE_STANDARD, 6667, aSpec, aOriginalCharset, aBaseURI);

    return uri.QueryInterface(Components.interfaces.nsIURI);
  },
  
  newChannel: function (aURI)
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    if (!ioService.allowPort(aURI.port, aURI.scheme))
      throw Components.results.NS_ERROR_FAILURE;
    
    return new nsDummyChannel(aURI);
  },  

  QueryInterface: function (aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIProtocolHandler) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    
    return this;        
  }
};

var nsBookmarkProtocolHandlerFactory = {
  createInstance: function (aOuter, aIID)
  {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    return (new nsBookmarkProtocolHandler()).QueryInterface(aIID);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Dummy Channel used by nsBookmarksProtocolHandler
function nsDummyChannel(aURI)
{
  this.URI = aURI;
  this.originalURI = aURI;
}

nsDummyChannel.prototype = {
  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function (aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIChannel) && 
        !aIID.equals(Components.interfaces.nsIRequest) && 
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // nsIChannel
  loadAttributes:         null,
  contentType:            "x-application-bookmark",
  contentLength:          0,
  owner:                  null,
  loadGroup:              null,
  notificationCallbacks:  null,
  securityInfo:           null,
  
  open: function ()
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  asyncOpen: function (aObserver, aContext)
  {
    aObserver.onStartRequest(this, aContext);
  },
  
  asyncRead: function (aListener, aContext)
  {
    return aListener.onStartRequest(this, aContext);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIRequest
  isPending: function () 
  { 
    return true;
  },
  
  _status: Components.results.NS_OK,
  cancel: function (aStatus) 
  { 
    this._status = aStatus; 
  },
  
  suspend: function ()
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  resume: function ()
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  }
};

///////////////////////////////////////////////////////////////////////////////
// Content handler for "x-application-bookmark"
function nsBookmarkContentHandler()
{
}

nsBookmarkContentHandler.prototype = {
  QueryInterface: function (aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIContentHandler))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },
  
  _bundle: null,
  
  handleContent: function (aContentType, aCommand, aWindowTarget, aRequest)
  {
    var ireq = aWindowTarget.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var win = ireq.getInterface(Components.interfaces.nsIDOMWindowInternal);
    
    // It'd be nicer if the add bookmark dialog was parameterizable enough 
    // to be able to open here, instead of just showing a prompt, but that can
    // be done later. 
    var spec = aRequest.QueryInterface(Components.interfaces.nsIChannel).URI.spec
    var bookmark = nsBookmarkProtocolUtils.parseBookmarkURL(spec);
    if (!bookmark) 
      return;

    if (!this._bundle) {
      var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(Components.interfaces.nsIStringBundleService);
      this._bundle = sbs.createBundle("chrome://browser/locale/bookmarks/bookmarks.properties");
    }
    
    var title = this._bundle.GetStringFromName("addBookmarkPromptTitle");
    var msg = this._bundle.formatStringFromName("addBookmarkPromptMessage", [bookmark.name, bookmark.url], 2);
    var addBookmarkStr = this._bundle.GetStringFromName("addBookmarkPromptButton");

    const nsIPromptService = Components.interfaces.nsIPromptService;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPromptService);
    var flags = (nsIPromptService.BUTTON_TITLE_IS_STRING * nsIPromptService.BUTTON_POS_0) + (nsIPromptService.BUTTON_TITLE_CANCEL * nsIPromptService.BUTTON_POS_1);
    var rv = promptService.confirmEx(win, title, msg, flags, addBookmarkStr, null, null, null, {value: 0});
    if (rv == 0) {
      var bms = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService(Components.interfaces.nsIBookmarksService);
      if (!bookmark.name) 
        bookmark.name = bookmark.url;
      
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
      var root = rdf.GetResource("NC:BookmarksRoot");  
      // Append this bookmark to the end.
      var bmRes = bms.createBookmarkInContainer(bookmark.name, bookmark.url, bookmark.keyword, 
                                                bookmark.description, bookmark.charset, root, -1);
      dump("*** bookmark =" + bookmark.toSource() + "\n");                                                
      var ds = bms.QueryInterface(Components.interfaces.nsIRDFDataSource);
      if (bookmark.webpanel == "true") {                                                   
        var webPanelArc = rdf.GetResource("http://home.netscape.com/NC-rdf#WebPanel");
        var trueLiteral = rdf.GetLiteral("true");
        ds.Assert(bmRes, webPanelArc, trueLiteral, true);
        var tgt =  ds.GetTarget(bmRes, webPanelArc, true);
        dump("*** tgt = "+ tgt.QueryInterface(Components.interfaces.nsIRDFLiteral).Value + "\n");
      }
                                                               
      var rds = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);                              
      rds.Flush();
    }
  }
}

var nsBookmarkContentHandlerFactory = {
  createInstance: function (aOuter, aIID)
  {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    if (!aIID.equals(Components.interfaces.nsIContentHandler) && 
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_INVALID_ARG;
    
    return new nsBookmarkContentHandler();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Module
var nsBookmarkProtocolModule = {
  _firstTime: true,
  
  registerSelf: function (aComponentManager, aFileSpec, aLocation, aType)
  {
    if (this._firstTime) {
      this._firstTime = false;
      throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
    }
    aComponentManager = aComponentManager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    aComponentManager.registerFactoryLocation(this._protocolHandlerCID, 
                                              "Bookmark Protocol Handler",
                                              this._protocolHandlerProgID,
                                              aFileSpec,
                                              aLocation,
                                              aType);
    aComponentManager.registerFactoryLocation(this._contentHandlerCID, 
                                              "Bookmark Content Handler",
                                              this._contentHandlerProgID,
                                              aFileSpec,
                                              aLocation,
                                              aType);
  },
  
  getClassObject: function (aComponentManager, aCID, aIID)
  {
    if (aCID.equals(this._protocolHandlerCID))
      return nsBookmarkProtocolHandlerFactory;
  
    if (aCID.equals(this._contentHandlerCID))
      return nsBookmarkContentHandlerFactory;

    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  

  _protocolHandlerCID: Components.ID("{4fd019a2-ec53-48cc-89ac-682e2a2b858d}"),
  _protocolHandlerProgID: "@mozilla.org/network/protocol;1?name=moz-bookmark",
  _contentHandlerCID: Components.ID("{99c35ecb-0b51-49de-b1c6-45e14bf9d506}"),
  _contentHandlerProgID: "@mozilla.org/uriloader/content-handler;1?type=x-application-bookmark",

  canUnload: function (aComponentManager)
  {
    return true;
  }
}

function NSGetModule(aComponentManager, aFileSpec)
{
  return nsBookmarkProtocolModule;
}

