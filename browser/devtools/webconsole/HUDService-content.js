/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This code is appended to the browser content script.
(function _HUDServiceContent() {
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

let tempScope = {};
Cu.import("resource://gre/modules/XPCOMUtils.jsm", tempScope);
Cu.import("resource://gre/modules/Services.jsm", tempScope);
Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm", tempScope);
Cu.import("resource:///modules/WebConsoleUtils.jsm", tempScope);

let XPCOMUtils = tempScope.XPCOMUtils;
let Services = tempScope.Services;
let gConsoleStorage = tempScope.ConsoleAPIStorage;
let WebConsoleUtils = tempScope.WebConsoleUtils;
let l10n = WebConsoleUtils.l10n;
tempScope = null;

let _alive = true; // Track if this content script should still be alive.

/**
 * The Web Console content instance manager.
 */
let Manager = {
  get window() content,
  get console() this.window.console,
  sandbox: null,
  hudId: null,
  _sequence: 0,
  _messageListeners: ["WebConsole:Init", "WebConsole:EnableFeature",
                      "WebConsole:DisableFeature", "WebConsole:Destroy"],
  _messageHandlers: null,
  _enabledFeatures: null,

  /**
   * Getter for a unique ID for the current Web Console content instance.
   */
  get sequenceId() "HUDContent-" + (++this._sequence),

  /**
   * Initialize the Web Console manager.
   */
  init: function Manager_init()
  {
    this._enabledFeatures = [];
    this._messageHandlers = {};

    this._messageListeners.forEach(function(aName) {
      addMessageListener(aName, this);
    }, this);

    // Need to track the owner XUL window to listen to the unload and TabClose
    // events, to avoid memory leaks.
    let xulWindow = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell)
                    .chromeEventHandler.ownerDocument.defaultView;

    xulWindow.addEventListener("unload", this._onXULWindowClose, false);

    let tabContainer = xulWindow.gBrowser.tabContainer;
    tabContainer.addEventListener("TabClose", this._onTabClose, false);

    // Need to track private browsing change and quit application notifications,
    // again to avoid memory leaks. The Web Console main process cannot notify
    // this content script when the XUL window close, tab close, private
    // browsing change and quit application events happen, so we must call
    // Manager.destroy() on our own.
    Services.obs.addObserver(this, "private-browsing-change-granted", false);
    Services.obs.addObserver(this, "quit-application-granted", false);
  },

  /**
   * The message handler. This method forwards all the remote messages to the
   * appropriate code.
   */
  receiveMessage: function Manager_receiveMessage(aMessage)
  {
    if (!_alive) {
      return;
    }

    if (!aMessage.json || (aMessage.name != "WebConsole:Init" &&
                           aMessage.json.hudId != this.hudId)) {
      Cu.reportError("Web Console content script: received message " +
                     aMessage.name + " from wrong hudId!");
      return;
    }

    switch (aMessage.name) {
      case "WebConsole:Init":
        this._onInit(aMessage.json);
        break;
      case "WebConsole:EnableFeature":
        this.enableFeature(aMessage.json.feature, aMessage.json);
        break;
      case "WebConsole:DisableFeature":
        this.disableFeature(aMessage.json.feature);
        break;
      case "WebConsole:Destroy":
        this.destroy();
        break;
      default: {
        let handler = this._messageHandlers[aMessage.name];
        handler && handler(aMessage.json);
        break;
      }
    }
  },

  /**
   * Observe notifications from the nsIObserverService.
   *
   * @param mixed aSubject
   * @param string aTopic
   * @param mixed aData
   */
  observe: function Manager_observe(aSubject, aTopic, aData)
  {
    if (_alive && (aTopic == "quit-application-granted" ||
        (aTopic == "private-browsing-change-granted" &&
         (aData == "enter" || aData == "exit")))) {
      this.destroy();
    }
  },

  /**
   * The manager initialization code. This method is called when the Web Console
   * remote process initializes the content process (this code!).
   *
   * @param object aMessage
   *        The object received from the remote process. The WebConsole:Init
   *        message properties:
   *        - hudId - (required) the remote Web Console instance ID.
   *        - features - (optional) array of features you want to enable from
   *        the start. For each feature you enable you can pass feature-specific
   *        options in a property on the JSON object you send with the same name
   *        as the feature. See this.enableFeature() for the list of available
   *        features.
   *        - cachedMessages - (optional) an array of cached messages you want
   *        to receive. See this._sendCachedMessages() for the list of available
   *        message types.
   *
   *        Example message:
   *        {
   *          hudId: "foo1",
   *          features: ["JSTerm", "ConsoleAPI"],
   *          ConsoleAPI: { ... }, // ConsoleAPI-specific options
   *          cachedMessages: ["ConsoleAPI"],
   *        }
   */
  _onInit: function Manager_onInit(aMessage)
  {
    this.hudId = aMessage.hudId;
    if (aMessage.features) {
      aMessage.features.forEach(function(aFeature) {
        this.enableFeature(aFeature, aMessage[aFeature]);
      }, this);
    }

    if (aMessage.cachedMessages) {
      this._sendCachedMessages(aMessage.cachedMessages);
    }
  },

  /**
   * Add a remote message handler. This is used by other components of the Web
   * Console content script.
   *
   * @param string aName
   *        Message name to listen for.
   * @param function aCallback
   *        Function to execute when the message is received. This function is
   *        given the JSON object that came from the remote Web Console
   *        instance.
   *        Only one callback per message name is allowed!
   */
  addMessageHandler: function Manager_addMessageHandler(aName, aCallback)
  {
    if (aName in this._messageHandlers) {
      Cu.reportError("Web Console content script: addMessageHandler() called for an existing message handler: " + aName);
      return;
    }

    this._messageHandlers[aName] = aCallback;
    addMessageListener(aName, this);
  },

  /**
   * Remove the message handler for the given name.
   *
   * @param string aName
   *        Message name for the handler you want removed.
   */
  removeMessageHandler: function Manager_removeMessageHandler(aName)
  {
    if (!(aName in this._messageHandlers)) {
      return;
    }

    delete this._messageHandlers[aName];
    removeMessageListener(aName, this);
  },

  /**
   * Send a message to the remote Web Console instance.
   *
   * @param string aName
   *        The name of the message you want to send.
   * @param object aMessage
   *        The message object you want to send.
   */
  sendMessage: function Manager_sendMessage(aName, aMessage)
  {
    aMessage.hudId = this.hudId;
    if (!("id" in aMessage)) {
      aMessage.id = this.sequenceId;
    }

    sendAsyncMessage(aName, aMessage);
  },

  /**
   * Enable a feature in the Web Console content script. A feature is generally
   * a set of observers/listeners that are added in the content process. This
   * content script exposes the data via the message manager for the features
   * you enable.
   *
   * Supported features:
   *    - JSTerm - a JavaScript "terminal" which allows code execution.
   *    - ConsoleAPI - support for routing the window.console API to the remote
   *    process.
   *    - PageError - route all the nsIScriptErrors from the nsIConsoleService
   *    to the remote process.
   *
   * @param string aFeature
   *        One of the supported features: JSTerm, ConsoleAPI.
   * @param object [aMessage]
   *        Optional JSON message object coming from the remote Web Console
   *        instance. This can be used for feature-specific options.
   */
  enableFeature: function Manager_enableFeature(aFeature, aMessage)
  {
    if (this._enabledFeatures.indexOf(aFeature) != -1) {
      return;
    }

    switch (aFeature) {
      case "JSTerm":
        JSTerm.init(aMessage);
        break;
      case "ConsoleAPI":
        ConsoleAPIObserver.init(aMessage);
        break;
      case "PageError":
        ConsoleListener.init(aMessage);
        break;
      default:
        Cu.reportError("Web Console content: unknown feature " + aFeature);
        break;
    }

    this._enabledFeatures.push(aFeature);
  },

  /**
   * Disable a Web Console content script feature.
   *
   * @see this.enableFeature
   * @param string aFeature
   *        One of the supported features - see this.enableFeature() for the
   *        list of supported features.
   */
  disableFeature: function Manager_disableFeature(aFeature)
  {
    let index = this._enabledFeatures.indexOf(aFeature);
    if (index == -1) {
      return;
    }
    this._enabledFeatures.splice(index, 1);

    switch (aFeature) {
      case "JSTerm":
        JSTerm.destroy();
        break;
      case "ConsoleAPI":
        ConsoleAPIObserver.destroy();
        break;
      case "PageError":
        ConsoleListener.destroy();
        break;
      default:
        Cu.reportError("Web Console content: unknown feature " + aFeature);
        break;
    }
  },

  /**
   * Send the cached messages to the remote Web Console instance.
   *
   * @private
   * @param array aMessageTypes
   *        An array that lists which kinds of messages you want. Supported
   *        message types: "ConsoleAPI" and "PageError".
   */
  _sendCachedMessages: function Manager__sendCachedMessages(aMessageTypes)
  {
    let messages = [];

    while (aMessageTypes.length > 0) {
      switch (aMessageTypes.shift()) {
        case "ConsoleAPI":
          messages.push.apply(messages, ConsoleAPIObserver.getCachedMessages());
          break;
        case "PageError":
          messages.push.apply(messages, ConsoleListener.getCachedMessages());
          break;
      }
    }

    messages.sort(function(a, b) { return a.timeStamp - b.timeStamp; });

    this.sendMessage("WebConsole:CachedMessages", {messages: messages});
  },

  /**
   * The XUL window "unload" event handler which destroys this content script
   * instance.
   * @private
   */
  _onXULWindowClose: function Manager__onXULWindowClose()
  {
    if (_alive) {
      Manager.destroy();
    }
  },

  /**
   * The "TabClose" event handler which destroys this content script
   * instance, if needed.
   * @private
   */
  _onTabClose: function Manager__onTabClose(aEvent)
  {
    let tab = aEvent.target;
    if (_alive && tab.linkedBrowser.contentWindow === Manager.window) {
      Manager.destroy();
    }
  },

  /**
   * Destroy the Web Console content script instance.
   */
  destroy: function Manager_destroy()
  {
    Services.obs.removeObserver(this, "private-browsing-change-granted");
    Services.obs.removeObserver(this, "quit-application-granted");

    _alive = false;
    let xulWindow = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell)
                    .chromeEventHandler.ownerDocument.defaultView;

    xulWindow.removeEventListener("unload", this._onXULWindowClose, false);
    let tabContainer = xulWindow.gBrowser.tabContainer;
    tabContainer.removeEventListener("TabClose", this._onTabClose, false);

    this._messageListeners.forEach(function(aName) {
      removeMessageListener(aName, this);
    }, this);

    this._enabledFeatures.slice().forEach(this.disableFeature, this);

    this.hudId = null;
    this._messageHandlers = null;
    Manager = ConsoleAPIObserver = JSTerm = ConsoleListener = null;
    Cc = Ci = Cu = XPCOMUtils = Services = gConsoleStorage =
      WebConsoleUtils = l10n = null;
  },
};

/**
 * The JavaScript terminal is meant to allow remote code execution for the Web
 * Console.
 */
let JSTerm = {
  /**
   * Evaluation result objects are cached in this object. The chrome process can
   * request any object based on its ID.
   */
  _objectCache: null,

  /**
   * Initialize the JavaScript terminal feature.
   */
  init: function JST_init()
  {
    this._objectCache = {};

    Manager.addMessageHandler("JSTerm:GetEvalObject",
                              this.handleGetEvalObject.bind(this));
    Manager.addMessageHandler("JSTerm:ClearObjectCache",
                              this.handleClearObjectCache.bind(this));
  },

  /**
   * Handler for the remote "JSTerm:GetEvalObject" message. This allows the
   * remote Web Console instance to retrieve an object from the content process.
   *
   * @param object aRequest
   *        The message that requests the content object. Properties: cacheId,
   *        objectId and resultCacheId.
   *
   *        Evaluated objects are stored in "buckets" (cache IDs). Each object
   *        is assigned an ID (object ID). You can request a specific object
   *        (objectId) from a specific cache (cacheId) and tell where the result
   *        should be cached (resultCacheId). The requested object can have
   *        further references to other objects - those references will be
   *        cached in the "bucket" of your choice (based on resultCacheId). If
   *        you do not provide any resultCacheId in the request message, then
   *        cacheId will be used.
   */
  handleGetEvalObject: function JST_handleGetEvalObject(aRequest)
  {
    if (aRequest.cacheId in this._objectCache &&
        aRequest.objectId in this._objectCache[aRequest.cacheId]) {
      let object = this._objectCache[aRequest.cacheId][aRequest.objectId];
      let resultCacheId = aRequest.resultCacheId || aRequest.cacheId;
      let message = {
        id: aRequest.id,
        cacheId: aRequest.cacheId,
        objectId: aRequest.objectId,
        object: this.prepareObjectForRemote(object, resultCacheId),
        childrenCacheId: resultCacheId,
      };
      Manager.sendMessage("JSTerm:EvalObject", message);
    }
    else {
      Cu.reportError("JSTerm:GetEvalObject request " + aRequest.id +
                     ": stale object.");
    }
  },

  /**
   * Handler for the remote "JSTerm:ClearObjectCache" message. This allows the
   * remote Web Console instance to clear the cache of objects that it no longer
   * uses.
   *
   * @param object aRequest
   *        An object that holds one property: the cacheId you want cleared.
   */
  handleClearObjectCache: function JST_handleClearObjectCache(aRequest)
  {
    if (aRequest.cacheId in this._objectCache) {
      delete this._objectCache[aRequest.cacheId];
    }
  },

  /**
   * Prepare an object to be sent to the remote Web Console instance.
   *
   * @param object aObject
   *        The object you want to send to the remote Web Console instance.
   * @param number aCacheId
   *        Cache ID where you want object references to be stored into. The
   *        given object may include references to other objects - those
   *        references will be stored in the given cache ID so the remote
   *        process can later retrieve them as well.
   * @return array
   *         An array that holds one element for each enumerable property and
   *         method in aObject. Each element describes the property. For details
   *         see WebConsoleUtils.namesAndValuesOf().
   */
  prepareObjectForRemote:
  function JST_prepareObjectForRemote(aObject, aCacheId)
  {
    // Cache the properties that have inspectable values.
    let propCache = this._objectCache[aCacheId] || {};
    let result = WebConsoleUtils.namesAndValuesOf(aObject, propCache);
    if (!(aCacheId in this._objectCache) && Object.keys(propCache).length > 0) {
      this._objectCache[aCacheId] = propCache;
    }

    return result;
  },

  /**
   * Destroy the JSTerm instance.
   */
  destroy: function JST_destroy()
  {
    Manager.removeMessageHandler("JSTerm:GetEvalObject");
    Manager.removeMessageHandler("JSTerm:ClearObjectCache");

    delete this._objectCache;
  },
};

/**
 * The window.console API observer. This allows the window.console API messages
 * to be sent to the remote Web Console instance.
 */
let ConsoleAPIObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  /**
   * Initialize the window.console API observer.
   */
  init: function CAO_init()
  {
    // Note that the observer is process-wide. We will filter the messages as
    // needed, see CAO_observe().
    Services.obs.addObserver(this, "console-api-log-event", false);

    Manager.addMessageHandler("ConsoleAPI:ClearCache",
                              this.handleClearCache.bind(this));
  },

  /**
   * The console API message observer. When messages are received from the
   * observer service we forward them to the remote Web Console instance.
   *
   * @param object aMessage
   *        The message object receives from the observer service.
   * @param string aTopic
   *        The message topic received from the observer service.
   */
  observe: function CAO_observe(aMessage, aTopic)
  {
    if (!_alive || !aMessage || aTopic != "console-api-log-event") {
      return;
    }

    let apiMessage = aMessage.wrappedJSObject;

    let msgWindow =
      WebConsoleUtils.getWindowByOuterId(apiMessage.ID, Manager.window);
    if (!msgWindow || msgWindow.top != Manager.window) {
      // Not the same window!
      return;
    }

    let messageToChrome = {};
    this._prepareApiMessageForRemote(apiMessage, messageToChrome);
    Manager.sendMessage("WebConsole:ConsoleAPI", messageToChrome);
  },

  /**
   * Prepare a message from the console APi to be sent to the remote Web Console
   * instance.
   *
   * @param object aOriginalMessage
   *        The original message received from console-api-log-event.
   * @param object aRemoteMessage
   *        The object you want to send to the remote Web Console. This object
   *        is updated to hold information from the original message. New
   *        properties added:
   *        - timeStamp
   *        Message timestamp (same as the aOriginalMessage.timeStamp property).
   *        - apiMessage
   *        An object that copies almost all the properties from
   *        aOriginalMessage. Arguments might be skipped if it holds references
   *        to objects that cannot be sent as they are to the remote Web Console
   *        instance.
   *        - argumentsToString
   *        Optional: the aOriginalMessage.arguments object stringified.
   *
   *        The apiMessage.arguments property is set to hold data appropriate
   *        to the message level. A similar approach is used for
   *        argumentsToString.
   */
  _prepareApiMessageForRemote:
  function CAO__prepareApiMessageForRemote(aOriginalMessage, aRemoteMessage)
  {
    aRemoteMessage.apiMessage =
      WebConsoleUtils.cloneObject(aOriginalMessage, true,
        function(aKey, aValue, aObject) {
          // We need to skip the arguments property from the original object.
          if (aKey == "wrappedJSObject" || aObject === aOriginalMessage &&
              aKey == "arguments") {
            return false;
          }
          return true;
        });

    aRemoteMessage.timeStamp = aOriginalMessage.timeStamp;

    switch (aOriginalMessage.level) {
      case "trace":
      case "time":
      case "timeEnd":
      case "group":
      case "groupCollapsed":
        aRemoteMessage.apiMessage.arguments =
          WebConsoleUtils.cloneObject(aOriginalMessage.arguments, true);
        break;

      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
      case "groupEnd":
        aRemoteMessage.argumentsToString =
          Array.map(aOriginalMessage.arguments || [],
                    this._formatObject.bind(this));
        break;

      case "dir": {
        aRemoteMessage.objectsCacheId = Manager.sequenceId;
        aRemoteMessage.argumentsToString = [];
        let mapFunction = function(aItem) {
          aRemoteMessage.argumentsToString.push(this._formatObject(aItem));
          if (WebConsoleUtils.isObjectInspectable(aItem)) {
            return JSTerm.prepareObjectForRemote(aItem,
                                                 aRemoteMessage.objectsCacheId);
          }
          return aItem;
        }.bind(this);

        aRemoteMessage.apiMessage.arguments =
          Array.map(aOriginalMessage.arguments || [], mapFunction);
        break;
      }
      default:
        Cu.reportError("Unknown Console API log level: " +
                       aOriginalMessage.level);
        break;
    }
  },

  /**
   * Format an object's value to be displayed in the Web Console.
   *
   * @private
   * @param object aObject
   *        The object you want to display.
   * @return string
   *         The string you can display for the given object.
   */
  _formatObject: function CAO__formatObject(aObject)
  {
    return typeof aObject == "string" ?
           aObject : WebConsoleUtils.formatResult(aObject);
  },

  /**
   * Get the cached messages for the current inner window.
   *
   * @see this._prepareApiMessageForRemote()
   * @return array
   *         The array of cached messages. Each element is a Console API
   *         prepared to be sent to the remote Web Console instance.
   */
  getCachedMessages: function CAO_getCachedMessages()
  {
    let innerWindowId = WebConsoleUtils.getInnerWindowId(Manager.window);
    let messages = gConsoleStorage.getEvents(innerWindowId);

    let result = messages.map(function(aMessage) {
      let remoteMessage = { _type: "ConsoleAPI" };
      this._prepareApiMessageForRemote(aMessage.wrappedJSObject, remoteMessage);
      return remoteMessage;
    }, this);

    return result;
  },

  /**
   * Handler for the "ConsoleAPI:ClearCache" message.
   */
  handleClearCache: function CAO_handleClearCache()
  {
    let windowId = WebConsoleUtils.getInnerWindowId(Manager.window);
    gConsoleStorage.clearEvents(windowId);
  },

  /**
   * Destroy the ConsoleAPIObserver listeners.
   */
  destroy: function CAO_destroy()
  {
    Manager.removeMessageHandler("ConsoleAPI:ClearCache");
    Services.obs.removeObserver(this, "console-api-log-event");
  },
};

/**
 * The nsIConsoleService listener. This is used to send all the page errors
 * (JavaScript, CSS and more) to the remote Web Console instance.
 */
let ConsoleListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),

  /**
   * Initialize the nsIConsoleService listener.
   */
  init: function CL_init()
  {
    Services.console.registerListener(this);
  },

  /**
   * The nsIConsoleService observer. This method takes all the script error
   * messages belonging to the current window and sends them to the remote Web
   * Console instance.
   *
   * @param nsIScriptError aScriptError
   *        The script error object coming from the nsIConsoleService.
   */
  observe: function CL_observe(aScriptError)
  {
    if (!_alive || !(aScriptError instanceof Ci.nsIScriptError) ||
        !aScriptError.outerWindowID) {
      return;
    }

    switch (aScriptError.category) {
      // We ignore chrome-originating errors as we only care about content.
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
        return;
    }

    let errorWindow =
      WebConsoleUtils.getWindowByOuterId(aScriptError.outerWindowID,
                                         Manager.window);
    if (!errorWindow || errorWindow.top != Manager.window) {
      return;
    }

    Manager.sendMessage("WebConsole:PageError", { pageError: aScriptError });
  },

  /**
   * Get the cached page errors for the current inner window.
   *
   * @return array
   *         The array of cached messages. Each element is an nsIScriptError
   *         with an added _type property so the remote Web Console instance can
   *         tell the difference between various types of cached messages.
   */
  getCachedMessages: function CL_getCachedMessages()
  {
    let innerWindowId = WebConsoleUtils.getInnerWindowId(Manager.window);
    let result = [];
    let errors = {};
    Services.console.getMessageArray(errors, {});

    (errors.value || []).forEach(function(aError) {
      if (!(aError instanceof Ci.nsIScriptError) ||
          aError.innerWindowID != innerWindowId) {
        return;
      }

      let remoteMessage = WebConsoleUtils.cloneObject(aError);
      remoteMessage._type = "PageError";
      result.push(remoteMessage);
    });

    return result;
  },

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy: function CL_destroy()
  {
    Services.console.unregisterListener(this);
  },
};

Manager.init();
})();
