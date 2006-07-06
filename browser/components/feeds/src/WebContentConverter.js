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
# The Original Code is the Web Content Converter System.
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

const WCCR_CONTRACTID = "@mozilla.org/embeddor.implemented/web-content-handler-registrar;1";
const WCCR_CLASSID = Components.ID("{792a7e82-06a0-437c-af63-b2d12e808acc}");
const WCCR_CLASSNAME = "Web Content Handler Registrar";

const WCC_CLASSID = Components.ID("{db7ebf28-cc40-415f-8a51-1b111851df1e}");
const WCC_CLASSNAME = "Web Service Handler";

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_ANY = "*/*";

const PREF_CONTENTHANDLERS_AUTO = "browser.contentHandlers.auto.";
const PREF_CONTENTHANDLERS_BRANCH = "browser.contentHandlers.types.";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_HANDLER = "browser.feeds.handler";

function WebContentConverter() {
}
WebContentConverter.prototype = {
  convert: function WCC_convert() { },
  asyncConvertData: function WCC_asyncConvertData() { },
  onDataAvailable: function WCC_onDataAvailable() { },
  onStopRequest: function WCC_onStopRequest() { },
  
  onStartRequest: function WCC_onStartRequest(request, context) {
    var wccr = 
        Cc[WCCR_CONTRACTID].
        getService(Ci.nsIWebContentConverterService);
    wccr.loadPreferredHandler(request);
  },
  
  QueryInterface: function WCC_QueryInterface(iid) {
    if (iid.equals(Ci.nsIStreamConverter) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var WebContentConverterFactory = {
  createInstance: function WCCF_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return new WebContentConverter().QueryInterface(iid);
  },
    
  QueryInterface: function WCC_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function ServiceInfo(contentType, uri, name) {
  this._contentType = contentType;
  this._uri = uri;
  this._name = name;
}
ServiceInfo.prototype = {
  /**
   * See nsIWebContentHandlerInfo
   */
  get contentType() {
    return this._contentType;
  },

  /**
   * See nsIWebContentHandlerInfo
   */
  get uri() {
    return this._uri;
  },

  /**
   * See nsIWebContentHandlerInfo
   */
  get name() {
    return this._name;
  },
  
  /**
   * See nsIWebContentHandlerInfo
   */
  getHandlerURI: function SI_getHandlerURI(uri) {
    return this._uri.replace(/%s/gi, encodeURIComponent(uri));
  },
  
  /**
   * See nsIWebContentHandlerInfo
   */
  equals: function SI_equals(other) {
    return this.contentType == other.contentType &&
           this.uri == other.uri;
  },
  
  QueryInterface: function SI_QueryInterface(iid) {
    if (iid.equals(Ci.nsIWebContentHandlerInfo) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var WebContentConverterRegistrar = {
  _contentTypes: { },
  _protocols: { },
  
  /**
   * Track auto handlers for various content types using a content-type to 
   * handler map.
   */
  _autoHandleContentTypes: { },

  /**
   * See nsIWebContentConverterService
   */
  getAutoHandler: 
  function WCCR_getAutoHandler(contentType) {
    contentType = this._resolveContentType(contentType);
    if (contentType in this._autoHandleContentTypes)
      return this._autoHandleContentTypes[contentType];
    return null;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  setAutoHandler:
  function WCCR_setAutoHandler(contentType, handler) {
    if (handler && !this._typeIsRegistered(contentType, handler.uri))
      throw Cr.NS_ERROR_NOT_AVAILABLE;
      
    contentType = this._resolveContentType(contentType);
    this._setAutoHandler(contentType, handler);
    
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    var autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
    if (handler)
      autoBranch.setCharPref(contentType, handler.uri);
    else if (autoBranch.prefHasUserValue(contentType))
      autoBranch.clearUserPref(contentType);
     
    ps.savePrefFile(null);
  },
  
  /**
   * Update the internal data structure (not persistent)
   */
  _setAutoHandler:
  function WCCR__setAutoHandler(contentType, handler) {
    if (handler) 
      this._autoHandleContentTypes[contentType] = handler;
    else if (contentType in this._autoHandleContentTypes)
      delete this._autoHandleContentTypes[contentType];
  },
  
  /**
   * See nsIWebContentConverterService
   */
  getWebContentHandlerByURI:
  function WCCR_getWebContentHandlerByURI(contentType, uri) {
    var handlers = this.getContentHandlers(contentType, { });
    for (var i = 0; i < handlers.length; ++i) {
      if (handlers[i].uri == uri) 
        return handlers[i];
    }
    return null;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  loadPreferredHandler: 
  function WCCR_loadPreferredHandler(request) {
    var channel = request.QueryInterface(Ci.nsIChannel);
    var contentType = this._resolveContentType(channel.contentType);
    var handler = this.getAutoHandler(contentType);
    if (handler) {
      request.cancel(Cr.NS_ERROR_FAILURE);
      
      var webNavigation = 
          channel.notificationCallbacks.getInterface(Ci.nsIWebNavigation);
      webNavigation.loadURI(handler.getHandlerURI(channel.URI.spec), 
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE, 
                            null, null, null);
    }      
  },
  
  /**
   * See nsIWebContentConverterService
   */
  removeProtocolHandler: 
  function WCCR_removeProtocolHandler(protocol, uri) {
    function notURI(currentURI) {
      return currentURI != uri;
    }
  
    if (protocol in this._protocols) 
      this._protocols[protocol] = this._protocols[protocol].filter(notURI);
  },
  
  /**
   * See nsIWebContentConverterService
   */
  removeContentHandler: 
  function WCCR_removeContentHandler(contentType, uri) {
    function notURI(serviceInfo) {
      return serviceInfo.uri != uri;
    }
  
    if (contentType in this._contentTypes) {
      this._contentTypes[contentType] = 
        this._contentTypes[contentType].filter(notURI);
    }
  },
  
  /**
   *
   */
  _mappings: { 
    "application/rss+xml": TYPE_MAYBE_FEED,
    "application/atom+xml": TYPE_MAYBE_FEED,
  },
  
  /**
   * These are types for which there is a separate content converter aside 
   * from our built in generic one. We should not automatically register
   * a factory for creating a converter for these types.
   */
  _blockedTypes: {
    "application/vnd.mozilla.maybe.feed": true,
  },
  
  /**
   * Determines the "internal" content type based on the _mappings.
   * @param   contentType
   * @returns The resolved contentType value. 
   */
  _resolveContentType: 
  function WCCR__resolveContentType(contentType) {
    if (contentType in this._mappings)
      return this._mappings[contentType];
    return contentType;
  },
  
  _wrapString: function WCCR__wrapString(string) {
    var supportsString = 
        Cc["@mozilla.org/supports-string;1"].
        createInstance(Ci.nsISupportsString);
    supportsString.data = string;
    return supportsString;
  },
  
  _updateDefaultReader: function WCCR__updateDefaultReader(uri) {
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var localizedString = 
        Cc["@mozilla.org/pref-localizedstring;1"].
        createInstance(Ci.nsIPrefLocalizedString);
    localizedString.data = uri;
    ps.setComplexValue(PREF_SELECTED_WEB, Ci.nsIPrefLocalizedString, 
                       localizedString);
    
    var needToUpdateHandler = true;
    try {
      needToUpdateHandler = ps.getCharPref(PREF_SELECTED_HANDLER) != "web";
    }
    catch (e) {
    }
    if (needToUpdateHandler)
      ps.setCharPref(PREF_SELECTED_HANDLER, "web");
  },
  
  _confirmAddHandler: function WCCR__confirmAddHandler(contentType, title, uri) {
    var args =
        Cc["@mozilla.org/supports-array;1"].
        createInstance(Ci.nsISupportsArray);
    
    var paramBlock = 
        Cc["@mozilla.org/embedcomp/dialogparam;1"].
        createInstance(Ci.nsIDialogParamBlock);
    // Used to tell the WCCR that the user chose to add the handler (rather 
    // than canceling) and whether or not they made it their default handler.
    const PARAM_SHOULD_ADD_HANDLER = 0;
    const PARAM_SHOULD_MAKE_DEFAULT = 1;
    paramBlock.SetInt(PARAM_SHOULD_ADD_HANDLER, 0);
    paramBlock.SetInt(PARAM_SHOULD_MAKE_DEFAULT, 0);
    args.AppendElement(paramBlock);
    args.AppendElement(this._wrapString(uri));
    args.AppendElement(this._wrapString(title));
    args.AppendElement(this._wrapString(contentType));
    
    var typeType = 
        Cc["@mozilla.org/supports-PRInt32;1"].
        createInstance(Ci.nsISupportsPRInt32);
    typeType.data = 1;
    args.AppendElement(typeType);
    
    var ww = 
        Cc["@mozilla.org/embedcomp/window-watcher;1"].
        getService(Ci.nsIWindowWatcher);
    ww.openWindow(null, "chrome://browser/content/feeds/addFeedReader.xul", 
                  "", "modal,titlebar,centerscreen,dialog=yes", args);
    
    var shouldAdd = paramBlock.GetInt(PARAM_SHOULD_ADD_HANDLER) == 1;
    if (shouldAdd&& contentType == TYPE_MAYBE_FEED && 
        paramBlock.GetInt(PARAM_SHOULD_MAKE_DEFAULT) == 1) {
      // User chose to use the reader as their default, so update the 
      // chosen reader preference, too.
      this._updateDefaultReader(uri);
    }
    return shouldAdd;
  },
  
  _checkForDuplicateContentType: 
  function WCCR__checkForDuplicateContentType(contentType, uri, title) {
    contentType = this._resolveContentType(contentType);
    if (this._typeIsRegistered(contentType, uri)) {
      // Show a special dialog for the feed case (XXXben - generalize at some 
      // point to allow other types to register specialized prompts).
      this._confirmAddHandler(contentType, title, uri);
      return false;
    }
    return true;
  },
  
  /**
   * See nsIWebContentHandlerRegistrar
   */
  registerProtocolHandler: 
  function WCCR_registerProtocolHandler(protocol, uri, title) {
    // XXXben - for Firefox 2 we only support feed types
    return;
    
    LOG("registerProtocolHandler(" + protocol + "," + uri + "," + title + ")");
    if (this._confirmAddHandler(protocol, title, uri))
      this._protocols[protocol] = uri;
  },

  /**
   * See nsIWebContentHandlerRegistrar
   * This is the web front end into the registration system, so a prompt to 
   * confirm the registration is provided, and the result is saved to 
   * preferences.
   */
  registerContentHandler: 
  function WCCR_registerContentHandler(contentType, uri, title) {
    LOG("registerContentHandler(" + contentType + "," + uri + "," + title + ")");
    
    // XXXben - for Firefox 2 we only support feed types
    contentType = this._resolveContentType(contentType);
    if (contentType != TYPE_MAYBE_FEED)
      return;    

    if (!this._checkForDuplicateContentType(contentType, uri, title) ||
        !this._confirmAddHandler(contentType, title, uri))
      return;
    
    // Reset the auto handler so that the user is asked again the next time
    // they load content of this type.
    if (this.getAutoHandler(contentType)) 
      this.setAutoHandler(contentType, null);

    this._registerContentHandler(contentType, uri, title);
    this._saveContentHandlerToPrefs(contentType, uri, title);    
  },
  
  /**
   * Save Web Content Handler metadata to persistent preferences. 
   * @param   contentType
   *          The content Type being handled
   * @param   uri
   *          The uri of the web service
   * @param   title
   *          The human readable name of the web service
   *
   * This data is stored under:
   * 
   *    browser.contentHandlers.type0 = content/type
   *    browser.contentHandlers.uri0 = http://www.foo.com/q=%s
   *    browser.contentHandlers.title0 = Foo 2.0alphr
   */
  _saveContentHandlerToPrefs: 
  function WCCR__saveContentHandlerToPrefs(contentType, uri, title) {
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    var i = 0;
    var typeBranch = null;
    while (true) {
      typeBranch = 
        ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + i + ".");
      try {
        typeBranch.getCharPref("type");
        ++i;
      }
      catch (e) {
        // No more handlers
        break;
      }
    }
    if (typeBranch) {
      typeBranch.setCharPref("type", contentType);
      typeBranch.setCharPref("uri", uri);
      typeBranch.setCharPref("title", title);
    
      ps.savePrefFile(null);
    }
  },
  
  /**
   * Determines if there is a type with a particular uri registered for the 
   * specified content type already.
   * @param   contentType
   *          The content type that the uri handles
   * @param   uri
   *          The uri of the 
   */
  _typeIsRegistered: function WCCR__typeIsRegistered(contentType, uri) {
    if (!(contentType in this._contentTypes))
      return false;
      
    var services = this._contentTypes[contentType];
    for (var i = 0; i < services.length; ++i) {
      // This uri has already been registered
      if (services[i].uri == uri)
        return true;
    }
    return false;
  },
  
  /**
   * Gets a stream converter contract id for the specified content type.
   * @param   contentType
   *          The source content type for the conversion.
   * @returns A contract id to construct a converter to convert between the 
   *          contentType and *\/*.
   */
  _getConverterContractID: function WCCR__getConverterContractID(contentType) {
    const template = "@mozilla.org/streamconv;1?from=%s&to=*/*";
    return template.replace(/%s/, contentType);
  },
  
  /**
   * Update the content type -> handler map. This mapping is not persisted, use
   * registerContentHandler or _saveContentHandlerToPrefs for that purpose.
   * @param   contentType
   *          The content Type being handled
   * @param   uri
   *          The uri of the web service
   * @param   title
   *          The human readable name of the web service
   */
  _registerContentHandler: 
  function WCCR__registerContentHandler(contentType, uri, title) {
    if (!(contentType in this._contentTypes))
      this._contentTypes[contentType] = [];

    // Avoid adding duplicates
    if (this._typeIsRegistered(contentType, uri)) 
      return;
    
    this._contentTypes[contentType].push(new ServiceInfo(contentType, uri, title));
    
    if (!(contentType in this._blockedTypes)) {
      var converterContractID = this._getConverterContractID(contentType);
      var cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      cr.registerFactory(WCC_CLASSID, WCC_CLASSNAME, converterContractID, 
                         WebContentConverterFactory);
    }
  },
  
  /**
   * See nsIWebContentConverterService
   */
  getContentHandlers: 
  function WCCR_getContentHandlers(contentType, countRef) {
    countRef.value = 0;
    if (!(contentType in this._contentTypes))
      return [];
    
    var handlers = this._contentTypes[contentType];
    countRef.value = handlers.length;
    return handlers;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  resetHandlersForType: 
  function WCCR_resetHandlersForType(contentType) {
    contentType = this._resolveContentType(contentType);
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    try {
      var i = 0;
      while (true) {
        var handlerBranch = 
          ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + i + ".");
        try {
          if (handlerBranch.getCharPref("type") == contentType)
            handlerBranch.resetBranch("");
          var defaultBranch = 
              ps.getDefaultBranch(PREF_CONTENTHANDLERS_BRANCH + i + ".");
          if (!this._registerContentHandlerWithBranch(defaultBranch))
            break;
          ++i;
        }
        catch (e) {
        }
      }
    }
    catch (e) {
    }
    ps.savePrefFile(null);
  },
  
  /**
   * Registers a handler from the settings on a branch
   */
  _registerContentHandlerWithBranch: function(branch) {
    try {
      var type = branch.getCharPref("type");
      var uri = branch.getCharPref("uri");
      var title = branch.getCharPref("title");
      this._registerContentHandler(type, uri, title);
    }
    catch (e) {
      return false;
    }
    return true;
  },
  
  /**
   * Load the auto handler, content handler and protocol tables from 
   * preferences.
   */
  _init: function WCCR__init() {
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    try {
      var i = 0;
      while (true) {
        var handlerBranch = 
          ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + (i++) + ".");
        if (!this._registerContentHandlerWithBranch(handlerBranch))
          break;
      }
    }
    catch (e) {
      // No content handlers yet, that's fine
      //LOG("WCCR.init: There are no content handlers registered in preferences (benign).");
    }

    // We need to do this _after_ registering all of the available handlers, 
    // so that getWebContentHandlerByURI can return successfully.
    try {
      var autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
      var childPrefs = autoBranch.getChildList("", { });
      for (var i = 0; i < childPrefs.length; ++i) {
        var type = childPrefs[i];
        var uri = autoBranch.getCharPref(type);
        if (uri) {
          var handler = this.getWebContentHandlerByURI(type, uri);
          this._setAutoHandler(type, handler);
        }
      }
    }
    catch (e) {
      // No auto branch yet, that's fine
      //LOG("WCCR.init: There is no auto branch, benign");
    }
  },
  
  /**
   * See nsIObserver
   */
  observe: function WCCR_observe(subject, topic, data) {
    var os = 
        Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    switch (topic) {
    case "app-startup":
      os.addObserver(this, "profile-after-change", false);
      break;
    case "profile-after-change":
      os.removeObserver(this, "profile-after-change");
      this._init();
      break;      
    }
  },
  
  /**
   * See nsIFactory
   */
  createInstance: function WCCR_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  /**
   * See nsIClassInfo
   */
  getInterfaces: function WCCR_getInterfaces(countRef) {
    var interfaces = 
        [Ci.nsIWebContentConverterService, Ci.nsIWebContentHandlerRegistrar,
         Ci.nsIObserver, Ci.nsIClassInfo, Ci.nsIFactory, Ci.nsISupports];
    countRef.value = interfaces.length;
    return interfaces;
  },
  getHelperForLanguage: function WCCR_getHelperForLanguage(language) {
    return null;
  },
  contractID: WCCR_CONTRACTID,
  classDescription: WCCR_CLASSNAME,
  classID: WCCR_CLASSID,
  implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
  flags: Ci.nsIClassInfo.DOM_OBJECT,
  
  /**
   * See nsISupports
   */
  QueryInterface: function WCCR_QueryInterface(iid) {
    if (iid.equals(Ci.nsIWebContentConverterService) || 
        iid.equals(Ci.nsIWebContentHandlerRegistrar) ||
        iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsIClassInfo) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
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
    
    if (cid.equals(WCCR_CLASSID))
      return WebContentConverterRegistrar;
      
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  registerSelf: function M_registerSelf(cm, file, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    cr.registerFactoryLocation(WCCR_CLASSID, WCCR_CLASSNAME, WCCR_CONTRACTID,
                               file, location, type);

    var catman = 
        Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catman.addCategoryEntry("app-startup", WCCR_CLASSNAME, 
                            "service," + WCCR_CONTRACTID, true, true, null);
  },
  
  unregisterSelf: function M_unregisterSelf(cm, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    cr.unregisterFactoryLocation(WCCR_CLASSID, location);
  },
  
  canUnload: function M_canUnload(cm) {
    return true;
  }
};

function NSGetModule(cm, file) {
  return Module;
}

#include ../../../../toolkit/content/debug.js

