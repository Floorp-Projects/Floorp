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
Cu.import("resource:///modules/NetworkHelper.jsm", tempScope);
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);

let XPCOMUtils = tempScope.XPCOMUtils;
let Services = tempScope.Services;
let gConsoleStorage = tempScope.ConsoleAPIStorage;
let WebConsoleUtils = tempScope.WebConsoleUtils;
let l10n = WebConsoleUtils.l10n;
let JSPropertyProvider = tempScope.JSPropertyProvider;
let NetworkHelper = tempScope.NetworkHelper;
let NetUtil = tempScope.NetUtil;
tempScope = null;

let activityDistributor = Cc["@mozilla.org/network/http-activity-distributor;1"].getService(Ci.nsIHttpActivityDistributor);

let _alive = true; // Track if this content script should still be alive.

/**
 * The Web Console content instance manager.
 */
let Manager = {
  get window() content,
  hudId: null,
  _sequence: 0,
  _messageListeners: ["WebConsole:Init", "WebConsole:EnableFeature",
                      "WebConsole:DisableFeature", "WebConsole:SetPreferences",
                      "WebConsole:GetPreferences", "WebConsole:Destroy"],
  _messageHandlers: null,
  _enabledFeatures: null,
  _prefs: { },

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
    let xulWindow = this._xulWindow();
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
    if (!_alive || !aMessage.json) {
      return;
    }

    if (aMessage.name == "WebConsole:Init" && !this.hudId) {
      this._onInit(aMessage.json);
      return;
    }
    if (aMessage.json.hudId != this.hudId) {
      return;
    }

    switch (aMessage.name) {
      case "WebConsole:EnableFeature":
        this.enableFeature(aMessage.json.feature, aMessage.json);
        break;
      case "WebConsole:DisableFeature":
        this.disableFeature(aMessage.json.feature);
        break;
      case "WebConsole:GetPreferences":
        this.handleGetPreferences(aMessage.json);
        break;
      case "WebConsole:SetPreferences":
        this.handleSetPreferences(aMessage.json);
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
   *        - preferences - (optional) an object of preferences you want to set.
   *        Use keys for preference names and values for preference values.
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
   *          preferences: {"foo.bar": true},
   *        }
   */
  _onInit: function Manager_onInit(aMessage)
  {
    this.hudId = aMessage.hudId;

    if (aMessage.preferences) {
      this.handleSetPreferences({ preferences: aMessage.preferences });
    }

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
   *    - NetworkMonitor - log all the network activity and send HAR-like
   *    messages to the remote Web Console process.
   *    - LocationChange - log page location changes. See
   *    ConsoleProgressListener.
   *
   * @param string aFeature
   *        One of the supported features.
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
      case "NetworkMonitor":
        NetworkMonitor.init(aMessage);
        break;
      case "LocationChange":
        ConsoleProgressListener.startMonitor(ConsoleProgressListener
                                             .MONITOR_LOCATION_CHANGE);
        ConsoleProgressListener.sendLocation(this.window.location.href,
                                             this.window.document.title);
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
      case "NetworkMonitor":
        NetworkMonitor.destroy();
        break;
      case "LocationChange":
        ConsoleProgressListener.stopMonitor(ConsoleProgressListener
                                            .MONITOR_LOCATION_CHANGE);
        break;
      default:
        Cu.reportError("Web Console content: unknown feature " + aFeature);
        break;
    }
  },

  /**
   * Handle the "WebConsole:GetPreferences" messages from the remote Web Console
   * instance.
   *
   * @param object aMessage
   *        The JSON object of the remote message. This object holds one
   *        property: preferences. The |preferences| value must be an array of
   *        preference names you want to retrieve the values for.
   *        A "WebConsole:Preferences" message is sent back to the remote Web
   *        Console instance. The message holds a |preferences| object which has
   *        key names for preference names and values for each preference value.
   */
  handleGetPreferences: function Manager_handleGetPreferences(aMessage)
  {
    let prefs = {};
    aMessage.preferences.forEach(function(aName) {
      prefs[aName] = this.getPreference(aName);
    }, this);

    this.sendMessage("WebConsole:Preferences", {preferences: prefs});
  },

  /**
   * Handle the "WebConsole:SetPreferences" messages from the remote Web Console
   * instance.
   *
   * @param object aMessage
   *        The JSON object of the remote message. This object holds one
   *        property: preferences. The |preferences| value must be an object of
   *        preference names as keys and preference values as object values, for
   *        each preference you want to change.
   */
  handleSetPreferences: function Manager_handleSetPreferences(aMessage)
  {
    for (let key in aMessage.preferences) {
      this.setPreference(key, aMessage.preferences[key]);
    }
  },

  /**
   * Retrieve a preference.
   *
   * @param string aName
   *        Preference name.
   * @return mixed|null
   *         Preference value. Null is returned if the preference does not
   *         exist.
   */
  getPreference: function Manager_getPreference(aName)
  {
    return aName in this._prefs ? this._prefs[aName] : null;
  },

  /**
   * Set a preference to a new value.
   *
   * @param string aName
   *        Preference name.
   * @param mixed aValue
   *        Preference value.
   */
  setPreference: function Manager_setPreference(aName, aValue)
  {
    this._prefs[aName] = aValue;
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
   * Find the XUL window that owns the content script.
   * @private
   * @return Window
   *         The XUL window that owns the content script.
   */
  _xulWindow: function Manager__xulWindow()
  {
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIWebNavigation).QueryInterface(Ci.nsIDocShell)
           .chromeEventHandler.ownerDocument.defaultView;
  },

  /**
   * Destroy the Web Console content script instance.
   */
  destroy: function Manager_destroy()
  {
    Services.obs.removeObserver(this, "private-browsing-change-granted");
    Services.obs.removeObserver(this, "quit-application-granted");

    _alive = false;
    let xulWindow = this._xulWindow();
    xulWindow.removeEventListener("unload", this._onXULWindowClose, false);
    let tabContainer = xulWindow.gBrowser.tabContainer;
    tabContainer.removeEventListener("TabClose", this._onTabClose, false);

    this._messageListeners.forEach(function(aName) {
      removeMessageListener(aName, this);
    }, this);

    this._enabledFeatures.slice().forEach(this.disableFeature, this);

    this.hudId = null;
    this._messageHandlers = null;

    Manager = ConsoleAPIObserver = JSTerm = ConsoleListener = NetworkMonitor =
      NetworkResponseListener = ConsoleProgressListener = null;

    XPCOMUtils = gConsoleStorage = WebConsoleUtils = l10n = JSPropertyProvider =
      null;
  },
};

///////////////////////////////////////////////////////////////////////////////
// JavaScript Terminal
///////////////////////////////////////////////////////////////////////////////

/**
 * JSTerm helper functions.
 *
 * Defines a set of functions ("helper functions") that are available from the
 * Web Console but not from the web page.
 *
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 */
function JSTermHelper(aJSTerm)
{
  /**
   * Find a node by ID.
   *
   * @param string aId
   *        The ID of the element you want.
   * @return nsIDOMNode or null
   *         The result of calling document.getElementById(aId).
   */
  aJSTerm.sandbox.$ = function JSTH_$(aId)
  {
    return aJSTerm.window.document.getElementById(aId);
  };

  /**
   * Find the nodes matching a CSS selector.
   *
   * @param string aSelector
   *        A string that is passed to window.document.querySelectorAll.
   * @return nsIDOMNodeList
   *         Returns the result of document.querySelectorAll(aSelector).
   */
  aJSTerm.sandbox.$$ = function JSTH_$$(aSelector)
  {
    return aJSTerm.window.document.querySelectorAll(aSelector);
  };

  /**
   * Runs an xPath query and returns all matched nodes.
   *
   * @param string aXPath
   *        xPath search query to execute.
   * @param [optional] nsIDOMNode aContext
   *        Context to run the xPath query on. Uses window.document if not set.
   * @returns array of nsIDOMNode
   */
  aJSTerm.sandbox.$x = function JSTH_$x(aXPath, aContext)
  {
    let nodes = [];
    let doc = aJSTerm.window.document;
    let aContext = aContext || doc;

    try {
      let results = doc.evaluate(aXPath, aContext, null,
                                 Ci.nsIDOMXPathResult.ANY_TYPE, null);
      let node;
      while (node = results.iterateNext()) {
        nodes.push(node);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }

    return nodes;
  };

  /**
   * Returns the currently selected object in the highlighter.
   *
   * Warning: this implementation crosses the process boundaries! This is not
   * usable within a remote browser. To implement this feature correctly we need
   * support for remote inspection capabilities within the Inspector as well.
   *
   * @return nsIDOMElement|null
   *         The DOM element currently selected in the highlighter.
   */
  Object.defineProperty(aJSTerm.sandbox, "$0", {
    get: function() {
      try {
        return Manager._xulWindow().InspectorUI.selection;
      }
      catch (ex) {
        aJSTerm.console.error(ex.message);
      }
    },
    enumerable: true,
    configurable: false
  });

  /**
   * Clears the output of the JSTerm.
   */
  aJSTerm.sandbox.clear = function JSTH_clear()
  {
    aJSTerm.helperEvaluated = true;
    Manager.sendMessage("JSTerm:ClearOutput", {});
  };

  /**
   * Returns the result of Object.keys(aObject).
   *
   * @param object aObject
   *        Object to return the property names from.
   * @returns array of string
   */
  aJSTerm.sandbox.keys = function JSTH_keys(aObject)
  {
    return Object.keys(WebConsoleUtils.unwrap(aObject));
  };

  /**
   * Returns the values of all properties on aObject.
   *
   * @param object aObject
   *        Object to display the values from.
   * @returns array of string
   */
  aJSTerm.sandbox.values = function JSTH_values(aObject)
  {
    let arrValues = [];
    let obj = WebConsoleUtils.unwrap(aObject);

    try {
      for (let prop in obj) {
        arrValues.push(obj[prop]);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
    return arrValues;
  };

  /**
   * Opens a help window in MDN.
   */
  aJSTerm.sandbox.help = function JSTH_help()
  {
    aJSTerm.helperEvaluated = true;
    aJSTerm.window.open(
        "https://developer.mozilla.org/AppLinks/WebConsoleHelp?locale=" +
        aJSTerm.window.navigator.language, "help", "");
  };

  /**
   * Inspects the passed aObject. This is done by opening the PropertyPanel.
   *
   * @param object aObject
   *        Object to inspect.
   */
  aJSTerm.sandbox.inspect = function JSTH_inspect(aObject)
  {
    if (!WebConsoleUtils.isObjectInspectable(aObject)) {
      return aObject;
    }

    aJSTerm.helperEvaluated = true;

    let message = {
      input: aJSTerm._evalInput,
      objectCacheId: Manager.sequenceId,
    };

    message.resultObject =
      aJSTerm.prepareObjectForRemote(WebConsoleUtils.unwrap(aObject),
                                     message.objectCacheId);

    Manager.sendMessage("JSTerm:InspectObject", message);
  };

  /**
   * Prints aObject to the output.
   *
   * @param object aObject
   *        Object to print to the output.
   * @return string
   */
  aJSTerm.sandbox.pprint = function JSTH_pprint(aObject)
  {
    aJSTerm.helperEvaluated = true;
    if (aObject === null || aObject === undefined || aObject === true ||
        aObject === false) {
      aJSTerm.console.error(l10n.getStr("helperFuncUnsupportedTypeError"));
      return;
    }
    else if (typeof aObject == "function") {
      aJSTerm.helperRawOutput = true;
      return aObject + "\n";
    }

    aJSTerm.helperRawOutput = true;

    let output = [];
    let pairs = WebConsoleUtils.namesAndValuesOf(WebConsoleUtils.unwrap(aObject));
    pairs.forEach(function(aPair) {
      output.push(aPair.name + ": " + aPair.value);
    });

    return "  " + output.join("\n  ");
  };

  /**
   * Print a string to the output, as-is.
   *
   * @param string aString
   *        A string you want to output.
   * @returns void
   */
  aJSTerm.sandbox.print = function JSTH_print(aString)
  {
    aJSTerm.helperEvaluated = true;
    aJSTerm.helperRawOutput = true;
    return String(aString);
  };
}

/**
 * The JavaScript terminal is meant to allow remote code execution for the Web
 * Console.
 */
let JSTerm = {
  get window() Manager.window,
  get console() this.window.console,

  /**
   * The Cu.Sandbox() object where code is evaluated.
   */
  sandbox: null,

  _sandboxLocation: null,
  _messageHandlers: {},

  /**
   * Evaluation result objects are cached in this object. The chrome process can
   * request any object based on its ID.
   */
  _objectCache: null,

  /**
   * Initialize the JavaScript terminal feature.
   *
   * @param object aMessage
   *        Options for JSTerm sent from the remote Web Console instance. This
   *        object holds the following properties:
   *
   *          - notifyNonNativeConsoleAPI - boolean that tells if you want to be
   *          notified if the window.console API object in the page is not the
   *          native one (if the page overrides it).
   *          A "JSTerm:NonNativeConsoleAPI" message will be sent if this is the
   *          case.
   */
  init: function JST_init(aMessage)
  {
    this._objectCache = {};
    this._messageHandlers = {
      "JSTerm:EvalRequest": this.handleEvalRequest,
      "JSTerm:GetEvalObject": this.handleGetEvalObject,
      "JSTerm:Autocomplete": this.handleAutocomplete,
      "JSTerm:ClearObjectCache": this.handleClearObjectCache,
    };

    for (let name in this._messageHandlers) {
      let handler = this._messageHandlers[name].bind(this);
      Manager.addMessageHandler(name, handler);
    }

    if (aMessage && aMessage.notifyNonNativeConsoleAPI) {
      let consoleObject = WebConsoleUtils.unwrap(this.window).console;
      if (!("__mozillaConsole__" in consoleObject)) {
        Manager.sendMessage("JSTerm:NonNativeConsoleAPI", {});
      }
    }
  },

  /**
   * Handler for the "JSTerm:EvalRequest" remote message. This method evaluates
   * user input in the JavaScript sandbox and sends the result back to the
   * remote process. The "JSTerm:EvalResult" message includes the following
   * data:
   *   - id - the same ID as the EvalRequest (for tracking purposes).
   *   - input - the JS string that was evaluated.
   *   - resultString - the evaluation result converted to a string formatted
   *   for display.
   *   - timestamp - timestamp when evaluation occurred (Date.now(),
   *   milliseconds since the UNIX epoch).
   *   - inspectable - boolean that tells if the evaluation result object can be
   *   inspected or not.
   *   - error - the evaluation exception object (if any).
   *   - errorMessage - the exception object converted to a string (if any error
   *   occurred).
   *   - helperResult - boolean that tells if a JSTerm helper was evaluated.
   *   - helperRawOutput - boolean that tells if the helper evaluation result
   *   should be displayed as raw output.
   *
   *   If the result object is inspectable then two additional properties are
   *   included:
   *     - childrenCacheId - tells where child objects are cached. This is the
   *     same as aRequest.resultCacheId.
   *     - resultObject - the result object prepared for the remote process. See
   *     this.prepareObjectForRemote().
   *
   * @param object aRequest
   *        The code evaluation request object:
   *          - id - request ID.
   *          - str - string to evaluate.
   *          - resultCacheId - where to cache the evaluation child objects.
   */
  handleEvalRequest: function JST_handleEvalRequest(aRequest)
  {
    let id = aRequest.id;
    let input = aRequest.str;
    let result, error = null;
    let timestamp;

    this.helperEvaluated = false;
    this.helperRawOutput = false;
    this._evalInput = input;
    try {
      timestamp = Date.now();
      result = this.evalInSandbox(input);
    }
    catch (ex) {
      error = ex;
    }
    delete this._evalInput;

    let inspectable = !error && WebConsoleUtils.isObjectInspectable(result);
    let resultString = undefined;
    if (!error) {
      resultString = this.helperRawOutput ? result :
                     WebConsoleUtils.formatResult(result);
    }

    let message = {
      id: id,
      input: input,
      resultString: resultString,
      timestamp: timestamp,
      error: error,
      errorMessage: error ? String(error) : null,
      inspectable: inspectable,
      helperResult: this.helperEvaluated,
      helperRawOutput: this.helperRawOutput,
    };

    if (inspectable) {
      message.childrenCacheId = aRequest.resultCacheId;
      message.resultObject =
        this.prepareObjectForRemote(result, message.childrenCacheId);
    }

    Manager.sendMessage("JSTerm:EvalResult", message);
  },

  /**
   * Handler for the remote "JSTerm:GetEvalObject" message. This allows the
   * remote Web Console instance to retrieve an object from the content process.
   * The "JSTerm:EvalObject" message is sent back to the remote process:
   *   - id - the request ID, used to trace back to the initial request.
   *   - cacheId - the cache ID where the requested object is stored.
   *   - objectId - the ID of the object being sent.
   *   - object - the object representation prepared for remote inspection. See
   *   this.prepareObjectForRemote().
   *   - childrenCacheId - the cache ID where any child object of |object| are
   *   stored.
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
  prepareObjectForRemote: function JST_prepareObjectForRemote(aObject, aCacheId)
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
   * Handler for the "JSTerm:Autocomplete" remote message. This handler provides
   * completion results for user input. The "JSterm:AutocompleteProperties"
   * message is sent to the remote process:
   *   - id - the same as request ID.
   *   - input - the user input (same as in the request message).
   *   - matches - an array of matched properties (strings).
   *   - matchProp - the part that was used from the user input for finding the
   *   matches. For details see the JSPropertyProvider description and
   *   implementation.
   *
   *
   * @param object aRequest
   *        The remote request object which holds two properties: an |id| and
   *        the user |input|.
   */
  handleAutocomplete: function JST_handleAutocomplete(aRequest)
  {
    let result = JSPropertyProvider(this.window, aRequest.input) || {};
    let message = {
      id: aRequest.id,
      input: aRequest.input,
      matches: result.matches || [],
      matchProp: result.matchProp,
    };
    Manager.sendMessage("JSTerm:AutocompleteProperties", message);
  },

  /**
   * Create the JavaScript sandbox where user input is evaluated.
   * @private
   */
  _createSandbox: function JST__createSandbox()
  {
    this._sandboxLocation = this.window.location;
    this.sandbox = new Cu.Sandbox(this.window, {
      sandboxPrototype: this.window,
      wantXrays: false,
    });

    this.sandbox.console = this.console;

    JSTermHelper(this);
  },

  /**
   * Evaluates a string in the sandbox.
   *
   * @param string aString
   *        String to evaluate in the sandbox.
   * @return mixed
   *         The result of the evaluation.
   */
  evalInSandbox: function JST_evalInSandbox(aString)
  {
    // If the user changed to a different location, we need to update the
    // sandbox.
    if (this._sandboxLocation !== this.window.location) {
      this._createSandbox();
    }

    // The help function needs to be easy to guess, so we make the () optional
    if (aString.trim() == "help" || aString.trim() == "?") {
      aString = "help()";
    }

    let window = WebConsoleUtils.unwrap(this.sandbox.window);
    let $ = null, $$ = null;

    // We prefer to execute the page-provided implementations for the $() and
    // $$() functions.
    if (typeof window.$ == "function") {
      $ = this.sandbox.$;
      delete this.sandbox.$;
    }
    if (typeof window.$$ == "function") {
      $$ = this.sandbox.$$;
      delete this.sandbox.$$;
    }

    let result = Cu.evalInSandbox(aString, this.sandbox, "1.8",
                                  "Web Console", 1);

    if ($) {
      this.sandbox.$ = $;
    }
    if ($$) {
      this.sandbox.$$ = $$;
    }

    return result;
  },

  /**
   * Destroy the JSTerm instance.
   */
  destroy: function JST_destroy()
  {
    for (let name in this._messageHandlers) {
      Manager.removeMessageHandler(name);
    }

    delete this.sandbox;
    delete this._sandboxLocation;
    delete this._messageHandlers;
    delete this._objectCache;
  },
};

///////////////////////////////////////////////////////////////////////////////
// The window.console API observer
///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
// The page errors listener
///////////////////////////////////////////////////////////////////////////////

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

    if (!this.isCategoryAllowed(aScriptError.category)) {
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
   * Check if the given script error category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string aCategory
   *        The nsIScriptError category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed: function CL_isCategoryAllowed(aCategory)
  {
    switch (aCategory) {
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
        return false;
    }

    return true;
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
          aError.innerWindowID != innerWindowId ||
          !this.isCategoryAllowed(aError.category)) {
        return;
      }

      let remoteMessage = WebConsoleUtils.cloneObject(aError);
      remoteMessage._type = "PageError";
      result.push(remoteMessage);
    }, this);

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

///////////////////////////////////////////////////////////////////////////////
// Network logging
///////////////////////////////////////////////////////////////////////////////

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

// The maximum number of bytes a NetworkResponseListener can hold.
const RESPONSE_BODY_LIMIT = 1048576; // 1 MB

/**
 * The network response listener implements the nsIStreamListener and
 * nsIRequestObserver interfaces. This is used within the NetworkMonitor feature
 * to get the response body of the request.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @constructor
 * @param object aHttpActivity
 *        HttpActivity object associated with this request. Once the request is
 *        complete the aHttpActivity object is updated to include the response
 *        headers and body.
 */
function NetworkResponseListener(aHttpActivity) {
  this.receivedData = "";
  this.httpActivity = aHttpActivity;
  this.bodySize = 0;
}

NetworkResponseListener.prototype = {
  QueryInterface:
    XPCOMUtils.generateQI([Ci.nsIStreamListener, Ci.nsIInputStreamCallback,
                           Ci.nsIRequestObserver, Ci.nsISupports]),

  /**
   * This NetworkResponseListener tracks the NetworkMonitor.openResponses object
   * to find the associated uncached headers.
   * @private
   */
  _foundOpenResponse: false,

  /**
   * The response will be written into the outputStream of this nsIPipe.
   * Both ends of the pipe must be blocking.
   */
  sink: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * The network response body size.
   */
  bodySize: null,

  /**
   * The nsIRequest we are started for.
   */
  request: null,

  /**
   * Set the async listener for the given nsIAsyncInputStream. This allows us to
   * wait asynchronously for any data coming from the stream.
   *
   * @param nsIAsyncInputStream aStream
   *        The input stream from where we are waiting for data to come in.
   * @param nsIInputStreamCallback aListener
   *        The input stream callback you want. This is an object that must have
   *        the onInputStreamReady() method. If the argument is null, then the
   *        current callback is removed.
   * @return void
   */
  setAsyncListener: function NRL_setAsyncListener(aStream, aListener)
  {
    // Asynchronously wait for the stream to be readable or closed.
    aStream.asyncWait(aListener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * RESPONSE_BODY_LIMIT constant.
   *
   * Learn more about nsIStreamListener at:
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsIInputStream aInputStream
   * @param unsigned long aOffset
   * @param unsigned long aCount
   */
  onDataAvailable:
  function NRL_onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount)
  {
    this._findOpenResponse();
    let data = NetUtil.readInputStreamToString(aInputStream, aCount);

    this.bodySize += aCount;

    if (!this.httpActivity.meta.discardResponseBody &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData += NetworkHelper.
                           convertToUnicode(data, aRequest.contentCharset);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   */
  onStartRequest: function NRL_onStartRequest(aRequest)
  {
    this.request = aRequest;
    this._findOpenResponse();
    // Asynchronously wait for the data coming from the request.
    this.setAsyncListener(this.sink.inputStream, this);
  },

  /**
   * Handle the onStopRequest by closing the sink output stream.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   */
  onStopRequest: function NRL_onStopRequest()
  {
    this._findOpenResponse();
    this.sink.outputStream.close();
  },

  /**
   * Find the open response object associated to the current request. The
   * NetworkMonitor.httpResponseExaminer() method saves the response headers in
   * NetworkMonitor.openResponses. This method takes the data from the open
   * response object and puts it into the HTTP activity object, then sends it to
   * the remote Web Console instance.
   *
   * @private
   */
  _findOpenResponse: function NRL__findOpenResponse()
  {
    if (!_alive || this._foundOpenResponse) {
      return;
    }

    let openResponse = null;

    for each (let item in NetworkMonitor.openResponses) {
      if (item.channel === this.httpActivity.channel) {
        openResponse = item;
        break;
      }
    }

    if (!openResponse) {
      return;
    }
    this._foundOpenResponse = true;

    let logResponse = this.httpActivity.log.entries[0].response;
    logResponse.headers = openResponse.headers;
    logResponse.httpVersion = openResponse.httpVersion;
    logResponse.status = openResponse.status;
    logResponse.statusText = openResponse.statusText;
    if (openResponse.cookies) {
      logResponse.cookies = openResponse.cookies;
    }

    delete NetworkMonitor.openResponses[openResponse.id];

    this.httpActivity.meta.stages.push("http-on-examine-response");
    NetworkMonitor.sendActivity(this.httpActivity);
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   * @return void
   */
  onStreamClose: function NRL_onStreamClose()
  {
    if (!this.httpActivity) {
      return;
    }
    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    this._findOpenResponse();

    let meta = this.httpActivity.meta;
    let entry = this.httpActivity.log.entries[0];
    let request = entry.request;
    let response = entry.response;

    meta.stages.push("REQUEST_STOP");

    if (!meta.discardResponseBody && this.receivedData.length) {
      this._onComplete(this.receivedData);
    }
    else if (!meta.discardResponseBody && response.status == 304) {
      // Response is cached, so we load it from cache.
      let charset = this.request.contentCharset || this.httpActivity.charset;
      NetworkHelper.loadFromCache(request.url, charset,
                                  this._onComplete.bind(this));
    }
    else {
      this._onComplete();
    }
  },

  /**
   * Handler for when the response completes. This function cleans up the
   * response listener.
   *
   * @param string [aData]
   *        Optional, the received data coming from the response listener or
   *        from the cache.
   */
  _onComplete: function NRL__onComplete(aData)
  {
    let response = this.httpActivity.log.entries[0].response;

    try {
      response.bodySize = response.status != 304 ? this.request.contentLength : 0;
    }
    catch (ex) {
      response.bodySize = -1;
    }

    try {
      response.content = { mimeType: this.request.contentType };
    }
    catch (ex) {
      response.content = { mimeType: "" };
    }

    if (response.content.mimeType && this.request.contentCharset) {
      response.content.mimeType += "; charset=" + this.request.contentCharset;
    }

    response.content.size = this.bodySize || (aData || "").length;

    if (aData) {
      response.content.text = aData;
    }

    this.receivedData = "";

    if (_alive) {
      NetworkMonitor.sendActivity(this.httpActivity);
    }

    this.httpActivity.channel = null;
    this.httpActivity = null;
    this.sink = null;
    this.inputStream = null;
    this.request = null;
  },

  /**
   * The nsIInputStreamCallback for when the request input stream is ready -
   * either it has more data or it is closed.
   *
   * @param nsIAsyncInputStream aStream
   *        The sink input stream from which data is coming.
   * @returns void
   */
  onInputStreamReady: function NRL_onInputStreamReady(aStream)
  {
    if (!(aStream instanceof Ci.nsIAsyncInputStream) || !this.httpActivity) {
      return;
    }

    let available = -1;
    try {
      // This may throw if the stream is closed normally or due to an error.
      available = aStream.available();
    }
    catch (ex) { }

    if (available != -1) {
      if (available != 0) {
        // Note that passing 0 as the offset here is wrong, but the
        // onDataAvailable() method does not use the offset, so it does not
        // matter.
        this.onDataAvailable(this.request, null, aStream, 0, available);
      }
      this.setAsyncListener(aStream, this);
    }
    else {
      this.onStreamClose();
    }
  },
};

/**
 * The network monitor uses the nsIHttpActivityDistributor to monitor network
 * requests. The nsIObserverService is also used for monitoring
 * http-on-examine-response notifications. All network request information is
 * routed to the remote Web Console.
 */
let NetworkMonitor = {
  httpTransactionCodes: {
    0x5001: "REQUEST_HEADER",
    0x5002: "REQUEST_BODY_SENT",
    0x5003: "RESPONSE_START",
    0x5004: "RESPONSE_HEADER",
    0x5005: "RESPONSE_COMPLETE",
    0x5006: "TRANSACTION_CLOSE",

    0x804b0003: "STATUS_RESOLVING",
    0x804b000b: "STATUS_RESOLVED",
    0x804b0007: "STATUS_CONNECTING_TO",
    0x804b0004: "STATUS_CONNECTED_TO",
    0x804b0005: "STATUS_SENDING_TO",
    0x804b000a: "STATUS_WAITING_FOR",
    0x804b0006: "STATUS_RECEIVING_FROM"
  },

  harCreator: {
    name: Services.appinfo.name + " - Web Console",
    version: Services.appinfo.version,
  },

  // Network response bodies are piped through a buffer of the given size (in
  // bytes).
  responsePipeSegmentSize: null,

  /**
   * Whether to save the bodies of network requests and responses. Disabled by
   * default to save memory.
   */
  get saveRequestAndResponseBodies() {
    return Manager.getPreference("NetworkMonitor.saveRequestAndResponseBodies");
  },

  openRequests: null,
  openResponses: null,
  progressListener: null,

  /**
   * The network monitor initializer.
   *
   * @param object aMessage
   *        Initialization object sent by the remote Web Console instance. This
   *        object can hold one property: monitorFileActivity - a boolean that
   *        tells if monitoring of file:// requests should be enabled as well or
   *        not.
   */
  init: function NM_init(aMessage)
  {
    this.responsePipeSegmentSize = Services.prefs
                                   .getIntPref("network.buffer.cache.size");

    this.openRequests = {};
    this.openResponses = {};

    activityDistributor.addObserver(this);

    Services.obs.addObserver(this.httpResponseExaminer,
                             "http-on-examine-response", false);

    // Monitor file:// activity as well.
    if (aMessage && aMessage.monitorFileActivity) {
      ConsoleProgressListener.startMonitor(ConsoleProgressListener
                                           .MONITOR_FILE_ACTIVITY);
    }
  },

  /**
   * Observe notifications for the http-on-examine-response topic, coming from
   * the nsIObserverService.
   *
   * @param nsIHttpChannel aSubject
   * @param string aTopic
   * @returns void
   */
  httpResponseExaminer: function NM_httpResponseExaminer(aSubject, aTopic)
  {
    // The httpResponseExaminer is used to retrieve the uncached response
    // headers. The data retrieved is stored in openResponses. The
    // NetworkResponseListener is responsible with updating the httpActivity
    // object with the data from the new object in openResponses.

    if (!_alive || aTopic != "http-on-examine-response" ||
        !(aSubject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
    // Try to get the source window of the request.
    let win = NetworkHelper.getWindowForRequest(channel);
    if (!win || win.top !== Manager.window) {
      return;
    }

    let response = {
      id: Manager.sequenceId,
      channel: channel,
      headers: [],
      cookies: [],
    };

    let setCookieHeader = null;

    channel.visitResponseHeaders({
      visitHeader: function NM__visitHeader(aName, aValue) {
        let lowerName = aName.toLowerCase();
        if (lowerName == "set-cookie") {
          setCookieHeader = aValue;
        }
        response.headers.push({ name: aName, value: aValue });
      }
    });

    if (!response.headers.length) {
      return; // No need to continue.
    }

    if (setCookieHeader) {
      response.cookies = NetworkHelper.parseSetCookieHeader(setCookieHeader);
    }

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};

    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getResponseVersion(httpVersionMaj, httpVersionMin);

    response.status = channel.responseStatus;
    response.statusText = channel.responseStatusText;
    response.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                     httpVersionMin.value;

    NetworkMonitor.openResponses[response.id] = response;
  },

  /**
   * Begin observing HTTP traffic that originates inside the current tab.
   *
   * @see https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIHttpActivityObserver
   *
   * @param nsIHttpChannel aChannel
   * @param number aActivityType
   * @param number aActivitySubtype
   * @param number aTimestamp
   * @param number aExtraSizeData
   * @param string aExtraStringData
   */
  observeActivity:
  function NM_observeActivity(aChannel, aActivityType, aActivitySubtype,
                              aTimestamp, aExtraSizeData, aExtraStringData)
  {
    if (!_alive ||
        aActivityType != activityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        aActivityType != activityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {
      return;
    }

    if (!(aChannel instanceof Ci.nsIHttpChannel)) {
      return;
    }

    aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

    if (aActivitySubtype ==
        activityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER) {
      this._onRequestHeader(aChannel, aTimestamp, aExtraStringData);
      return;
    }

    // Iterate over all currently ongoing requests. If aChannel can't
    // be found within them, then exit this function.
    let httpActivity = null;
    for each (let item in this.openRequests) {
      if (item.channel === aChannel) {
        httpActivity = item;
        break;
      }
    }

    if (!httpActivity) {
      return;
    }

    let transCodes = this.httpTransactionCodes;

    // Store the time information for this activity subtype.
    if (aActivitySubtype in transCodes) {
      let stage = transCodes[aActivitySubtype];
      if (stage in httpActivity.timings) {
        httpActivity.timings[stage].last = aTimestamp;
      }
      else {
        httpActivity.meta.stages.push(stage);
        httpActivity.timings[stage] = {
          first: aTimestamp,
          last: aTimestamp,
        };
      }
    }

    switch (aActivitySubtype) {
      case activityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT:
        this._onRequestBodySent(httpActivity);
        break;
      case activityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER:
        this._onResponseHeader(httpActivity, aExtraStringData);
        break;
      case activityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE:
        this._onTransactionClose(httpActivity);
        break;
      default:
        break;
    }
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_HEADER. When a request starts the
   * headers are sent to the server. This method creates the |httpActivity|
   * object where we store the request and response information that is
   * collected through its lifetime.
   *
   * @private
   * @param nsIHttpChannel aChannel
   * @param number aTimestamp
   * @param string aExtraStringData
   * @return void
   */
  _onRequestHeader:
  function NM__onRequestHeader(aChannel, aTimestamp, aExtraStringData)
  {
    // Try to get the source window of the request.
    let win = NetworkHelper.getWindowForRequest(aChannel);
    if (!win || win.top !== Manager.window) {
      return;
    }

    let httpActivity = this.createActivityObject(aChannel);
    httpActivity.charset = win.document.characterSet; // see NM__onRequestBodySent()
    httpActivity.meta.stages.push("REQUEST_HEADER"); // activity stage (aActivitySubtype)

    httpActivity.timings.REQUEST_HEADER = {
      first: aTimestamp,
      last: aTimestamp
    };

    let entry = httpActivity.log.entries[0];
    entry.startedDateTime = new Date(Math.round(aTimestamp / 1000)).toISOString();

    let request = httpActivity.log.entries[0].request;

    let cookieHeader = null;

    // Copy the request header data.
    aChannel.visitRequestHeaders({
      visitHeader: function NM__visitHeader(aName, aValue)
      {
        if (aName == "Cookie") {
          cookieHeader = aValue;
        }
        request.headers.push({ name: aName, value: aValue });
      }
    });

    if (cookieHeader) {
      request.cookies = NetworkHelper.parseCookieHeader(cookieHeader);
    }

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};

    aChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    aChannel.getRequestVersion(httpVersionMaj, httpVersionMin);

    request.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                    httpVersionMin.value;

    request.headersSize = aExtraStringData.length;

    this._setupResponseListener(httpActivity);

    this.openRequests[httpActivity.id] = httpActivity;

    this.sendActivity(httpActivity);
  },

  /**
   * Create the empty HTTP activity object. This object is used for storing all
   * the request and response information.
   *
   * This is a HAR-like object. Conformance to the spec is not guaranteed at
   * this point.
   *
   * TODO: Bug 708717 - Add support for network log export to HAR
   *
   * @see http://www.softwareishard.com/blog/har-12-spec
   * @param nsIHttpChannel aChannel
   *        The HTTP channel for which the HTTP activity object is created.
   * @return object
   *         The new HTTP activity object.
   */
  createActivityObject: function NM_createActivityObject(aChannel)
  {
    return {
      hudId: Manager.hudId,
      id: Manager.sequenceId,
      channel: aChannel,
      charset: null, // see NM__onRequestHeader()
      meta: { // holds metadata about the activity object
        stages: [], // activity stages (aActivitySubtype)
        discardRequestBody: !this.saveRequestAndResponseBodies,
        discardResponseBody: !this.saveRequestAndResponseBodies,
      },
      timings: {}, // internal timing information, see NM_observeActivity()
      log: { // HAR-like object
        version: "1.2",
        creator: this.harCreator,
        // missing |browser| and |pages|
        entries: [{  // we only track one entry at a time
          connection: Manager.sequenceId, // connection ID
          startedDateTime: 0, // see NM__onRequestHeader()
          time: 0, // see NM__setupHarTimings()
          // missing |serverIPAddress| and |cache|
          request: {
            method: aChannel.requestMethod,
            url: aChannel.URI.spec,
            httpVersion: "", // see NM__onRequestHeader()
            headers: [], // see NM__onRequestHeader()
            cookies: [], // see NM__onRequestHeader()
            queryString: [], // never set
            headersSize: -1, // see NM__onRequestHeader()
            bodySize: -1, // see NM__onRequestBodySent()
            postData: null, // see NM__onRequestBodySent()
          },
          response: {
            status: 0, // see NM__onResponseHeader()
            statusText: "", // see NM__onResponseHeader()
            httpVersion: "", // see NM__onResponseHeader()
            headers: [], // see NM_httpResponseExaminer()
            cookies: [], // see NM_httpResponseExaminer()
            content: null, // see NRL_onStreamClose()
            redirectURL: "", // never set
            headersSize: -1, // see NM__onResponseHeader()
            bodySize: -1, // see NRL_onStreamClose()
          },
          timings: {}, // see NM__setupHarTimings()
        }],
      },
    };
  },

  /**
   * Setup the network response listener for the given HTTP activity. The
   * NetworkResponseListener is responsible for storing the response body.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are tracking.
   */
  _setupResponseListener: function NM__setupResponseListener(aHttpActivity)
  {
    let channel = aHttpActivity.channel;
    channel.QueryInterface(Ci.nsITraceableChannel);

    // The response will be written into the outputStream of this pipe.
    // This allows us to buffer the data we are receiving and read it
    // asynchronously.
    // Both ends of the pipe must be blocking.
    let sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

    // The streams need to be blocking because this is required by the
    // stream tee.
    sink.init(false, false, this.responsePipeSegmentSize, PR_UINT32_MAX, null);

    // Add listener for the response body.
    let newListener = new NetworkResponseListener(aHttpActivity);

    // Remember the input stream, so it isn't released by GC.
    newListener.inputStream = sink.inputStream;
    newListener.sink = sink;

    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
              createInstance(Ci.nsIStreamListenerTee);

    let originalListener = channel.setNewListener(tee);

    tee.init(originalListener, sink.outputStream, newListener);
  },

  /**
   * Send an HTTP activity object to the remote Web Console instance.
   * A WebConsole:NetworkActivity message is sent. The message holds two
   * properties:
   *   - meta - the |aHttpActivity.meta| object.
   *   - log - the |aHttpActivity.log| object.
   *
   * @param object aHttpActivity
   *        The HTTP activity object you want to send.
   */
  sendActivity: function NM_sendActivity(aHttpActivity)
  {
    Manager.sendMessage("WebConsole:NetworkActivity", {
      meta: aHttpActivity.meta,
      log: aHttpActivity.log,
    });
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_BODY_SENT. The request body is logged
   * here.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   */
  _onRequestBodySent: function NM__onRequestBodySent(aHttpActivity)
  {
    if (aHttpActivity.meta.discardRequestBody) {
      return;
    }

    let request = aHttpActivity.log.entries[0].request;

    let sentBody = NetworkHelper.
                   readPostTextFromRequest(aHttpActivity.channel,
                                           aHttpActivity.charset);

    if (!sentBody && request.url == Manager.window.location.href) {
      // If the request URL is the same as the current page URL, then
      // we can try to get the posted text from the page directly.
      // This check is necessary as otherwise the
      //   NetworkHelper.readPostTextFromPage()
      // function is called for image requests as well but these
      // are not web pages and as such don't store the posted text
      // in the cache of the webpage.
      sentBody = NetworkHelper.readPostTextFromPage(docShell,
                                                    aHttpActivity.charset);
    }
    if (!sentBody) {
      return;
    }

    request.postData = {
      mimeType: "", // never set
      params: [],  // never set
      text: sentBody,
    };

    request.bodySize = sentBody.length;

    this.sendActivity(aHttpActivity);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_RESPONSE_HEADER. This method stores
   * information about the response headers.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   * @param string aExtraStringData
   *        The uncached response headers.
   */
  _onResponseHeader:
  function NM__onResponseHeader(aHttpActivity, aExtraStringData)
  {
    // aExtraStringData contains the uncached response headers. The first line
    // contains the response status (e.g. HTTP/1.1 200 OK).
    //
    // Note: The response header is not saved here. Calling the
    // channel.visitResponseHeaders() methood at this point sometimes causes an
    // NS_ERROR_NOT_AVAILABLE exception.
    //
    // We could parse aExtraStringData to get the headers and their values, but
    // that is not trivial to do in an accurate manner. Hence, we save the
    // response headers in this.httpResponseExaminer().

    let response = aHttpActivity.log.entries[0].response;

    let headers = aExtraStringData.split(/\r\n|\n|\r/);
    let statusLine = headers.shift();

    let statusLineArray = statusLine.split(" ");
    response.httpVersion = statusLineArray.shift();
    response.status = statusLineArray.shift();
    response.statusText = statusLineArray.join(" ");
    response.headersSize = aExtraStringData.length;

    // Discard the response body for known response statuses.
    switch (parseInt(response.status)) {
      case HTTP_MOVED_PERMANENTLY:
      case HTTP_FOUND:
      case HTTP_SEE_OTHER:
      case HTTP_TEMPORARY_REDIRECT:
        aHttpActivity.meta.discardResponseBody = true;
        break;
    }

    this.sendActivity(aHttpActivity);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_TRANSACTION_CLOSE. This method updates the HAR
   * timing information on the HTTP activity object and clears the request
   * from the list of known open requests.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we work with.
   */
  _onTransactionClose: function NM__onTransactionClose(aHttpActivity)
  {
    this._setupHarTimings(aHttpActivity);
    this.sendActivity(aHttpActivity);
    delete this.openRequests[aHttpActivity.id];
  },

  /**
   * Update the HTTP activity object to include timing information as in the HAR
   * spec. The HTTP activity object holds the raw timing information in
   * |timings| - these are timings stored for each activity notification. The
   * HAR timing information is constructed based on these lower level data.
   *
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   */
  _setupHarTimings: function NM__setupHarTimings(aHttpActivity)
  {
    let timings = aHttpActivity.timings;
    let entry = aHttpActivity.log.entries[0];
    let harTimings = entry.timings;

    // Not clear how we can determine "blocked" time.
    harTimings.blocked = -1;

    // DNS timing information is available only in when the DNS record is not
    // cached.
    harTimings.dns = timings.STATUS_RESOLVING && timings.STATUS_RESOLVED ?
                     timings.STATUS_RESOLVED.last -
                     timings.STATUS_RESOLVING.first : -1;

    if (timings.STATUS_CONNECTING_TO && timings.STATUS_CONNECTED_TO) {
      harTimings.connect = timings.STATUS_CONNECTED_TO.last -
                           timings.STATUS_CONNECTING_TO.first;
    }
    else if (timings.STATUS_SENDING_TO) {
      harTimings.connect = timings.STATUS_SENDING_TO.first -
                           timings.REQUEST_HEADER.first;
    }
    else {
      harTimings.connect = -1;
    }

    if ((timings.STATUS_WAITING_FOR || timings.STATUS_RECEIVING_FROM) &&
        (timings.STATUS_CONNECTED_TO || timings.STATUS_SENDING_TO)) {
      harTimings.send = (timings.STATUS_WAITING_FOR ||
                         timings.STATUS_RECEIVING_FROM).first -
                        (timings.STATUS_CONNECTED_TO ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.send = -1;
    }

    if (timings.RESPONSE_START) {
      harTimings.wait = timings.RESPONSE_START.first -
                        (timings.REQUEST_BODY_SENT ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.wait = -1;
    }

    if (timings.RESPONSE_START && timings.RESPONSE_COMPLETE) {
      harTimings.receive = timings.RESPONSE_COMPLETE.last -
                           timings.RESPONSE_START.first;
    }
    else {
      harTimings.receive = -1;
    }

    entry.time = 0;
    for (let timing in harTimings) {
      let time = Math.max(Math.round(harTimings[timing] / 1000), -1);
      harTimings[timing] = time;
      if (time > -1) {
        entry.time += time;
      }
    }
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   */
  destroy: function NM_destroy()
  {
    Services.obs.removeObserver(this.httpResponseExaminer,
                                "http-on-examine-response");

    activityDistributor.removeObserver(this);

    ConsoleProgressListener.stopMonitor(ConsoleProgressListener
                                        .MONITOR_FILE_ACTIVITY);

    delete this.openRequests;
    delete this.openResponses;
  },
};

/**
 * A WebProgressListener that listens for location changes.
 *
 * This progress listener is used to track file loads and other kinds of
 * location changes.
 *
 * When a file:// URI is loaded a "WebConsole:FileActivity" message is sent to
 * the remote Web Console instance. The message JSON holds only one property:
 * uri (the file URI).
 *
 * When the current page location changes a "WebConsole:LocationChange" message
 * is sent. See ConsoleProgressListener.sendLocation() for details.
 */
let ConsoleProgressListener = {
  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor file loads.
   */
  MONITOR_FILE_ACTIVITY: 1,

  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor page location changes.
   */
  MONITOR_LOCATION_CHANGE: 2,

  /**
   * Tells if you want to monitor file activity.
   * @private
   * @type boolean
   */
  _fileActivity: false,

  /**
   * Tells if you want to monitor location changes.
   * @private
   * @type boolean
   */
  _locationChange: false,

  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Initialize the ConsoleProgressListener.
   * @private
   */
  _init: function CPL__init()
  {
    if (this._initialized) {
      return;
    }

    this._initialized = true;
    let webProgress = docShell.QueryInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_ALL);
  },

  /**
   * Start a monitor/tracker related to the current nsIWebProgressListener
   * instance.
   *
   * @param number aMonitor
   *        Tells what you want to track. Available constants:
   *        - this.MONITOR_FILE_ACTIVITY
   *          Track file loads.
   *        - this.MONITOR_LOCATION_CHANGE
   *          Track location changes for the top window.
   */
  startMonitor: function CPL_startMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = true;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = true;
        break;
      default:
        throw new Error("HUDService-content: unknown monitor type " +
                        aMonitor + " for the ConsoleProgressListener!");
    }
    this._init();
  },

  /**
   * Stop a monitor.
   *
   * @param number aMonitor
   *        Tells what you want to stop tracking. See this.startMonitor() for
   *        the list of constants.
   */
  stopMonitor: function CPL_stopMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = false;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = false;
        break;
      default:
        throw new Error("HUDService-content: unknown monitor type " +
                        aMonitor + " for the ConsoleProgressListener!");
    }

    if (!this._fileActivity && !this._locationChange) {
      this.destroy();
    }
  },

  onStateChange:
  function CPL_onStateChange(aProgress, aRequest, aState, aStatus)
  {
    if (!_alive) {
      return;
    }

    if (this._fileActivity) {
      this._checkFileActivity(aProgress, aRequest, aState, aStatus);
    }

    if (this._locationChange) {
      this._checkLocationChange(aProgress, aRequest, aState, aStatus);
    }
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity:
  function CPL__checkFileActivity(aProgress, aRequest, aState, aStatus)
  {
    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (aRequest instanceof Ci.imgIRequest) {
      let imgIRequest = aRequest.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    }
    else if (aRequest instanceof Ci.nsIChannel) {
      let nsIChannel = aRequest.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || !uri.schemeIs("file") && !uri.schemeIs("ftp")) {
      return;
    }

    Manager.sendMessage("WebConsole:FileActivity", {uri: uri.spec});
  },

  /**
   * Check if the current window.top location is changing, given the arguments
   * of nsIWebProgressListener.onStateChange. If that is the case, the remote
   * Web Console instance is notified.
   * @private
   */
  _checkLocationChange:
  function CPL__checkLocationChange(aProgress, aRequest, aState, aStatus)
  {
    let isStart = aState & Ci.nsIWebProgressListener.STATE_START;
    let isStop = aState & Ci.nsIWebProgressListener.STATE_STOP;
    let isNetwork = aState & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isWindow = aState & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isNetwork || !isWindow ||
        aProgress.DOMWindow != Manager.window) {
      return;
    }

    if (isStart && aRequest instanceof Ci.nsIChannel) {
      this.sendLocation(aRequest.URI.spec, "");
    }
    else if (isStop) {
      this.sendLocation(Manager.window.location.href,
                        Manager.window.document.title);
    }
  },

  onLocationChange: function() {},
  onStatusChange: function() {},
  onProgressChange: function() {},
  onSecurityChange: function() {},

  /**
   * Send the location of the current top window to the remote Web Console.
   * A "WebConsole:LocationChange" message is sent. The JSON object holds two
   * properties: location and title.
   *
   * @param string aLocation
   *        Current page address.
   * @param string aTitle
   *        Current page title.
   */
  sendLocation: function CPL_sendLocation(aLocation, aTitle)
  {
    let message = {
      "location": aLocation,
      "title": aTitle,
    };
    Manager.sendMessage("WebConsole:LocationChange", message);
  },

  /**
   * Destroy the ConsoleProgressListener.
   */
  destroy: function CPL_destroy()
  {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;
    this._fileActivity = false;
    this._locationChange = false;
    let webProgress = docShell.QueryInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  },
};

Manager.init();
})();
