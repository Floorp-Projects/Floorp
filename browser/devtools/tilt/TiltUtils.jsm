/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");

const STACK_THICKNESS = 15;

this.EXPORTED_SYMBOLS = ["TiltUtils"];

/**
 * Module containing various helper functions used throughout Tilt.
 */
this.TiltUtils = {};

/**
 * Various console/prompt output functions required by the engine.
 */
TiltUtils.Output = {

  /**
   * Logs a message to the console.
   *
   * @param {String} aMessage
   *                 the message to be logged
   */
  log: function TUO_log(aMessage)
  {
    if (this.suppressLogs) {
      return;
    }
    // get the console service
    let consoleService = Cc["@mozilla.org/consoleservice;1"]
      .getService(Ci.nsIConsoleService);

    // log the message
    consoleService.logStringMessage(aMessage);
  },

  /**
   * Logs an error to the console.
   *
   * @param {String} aMessage
   *                 the message to be logged
   * @param {Object} aProperties
   *                 and object containing script error initialization details
   */
  error: function TUO_error(aMessage, aProperties)
  {
    if (this.suppressErrors) {
      return;
    }
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // get the console service
    let consoleService = Cc["@mozilla.org/consoleservice;1"]
      .getService(Ci.nsIConsoleService);

    // get the script error service
    let scriptError = Cc["@mozilla.org/scripterror;1"]
      .createInstance(Ci.nsIScriptError);

    // initialize a script error
    scriptError.init(aMessage,
      aProperties.sourceName || "",
      aProperties.sourceLine || "",
      aProperties.lineNumber || 0,
      aProperties.columnNumber || 0,
      aProperties.flags || 0,
      aProperties.category || "");

    // log the error
    consoleService.logMessage(scriptError);
  },

  /**
   * Shows a modal alert message popup.
   *
   * @param {String} aTitle
   *                 the title of the popup
   * @param {String} aMessage
   *                 the message to be logged
   */
  alert: function TUO_alert(aTitle, aMessage)
  {
    if (this.suppressAlerts) {
      return;
    }
    if (!aMessage) {
      aMessage = aTitle;
      aTitle = "";
    }

    // get the prompt service
    let prompt = Cc["@mozilla.org/embedcomp/prompt-service;1"]
      .getService(Ci.nsIPromptService);

    // show the alert message
    prompt.alert(null, aTitle, aMessage);
  }
};

/**
 * Helper functions for managing preferences.
 */
TiltUtils.Preferences = {

  /**
   * Gets a custom Tilt preference.
   * If the preference does not exist, undefined is returned. If it does exist,
   * but the type is not correctly specified, null is returned.
   *
   * @param {String} aPref
   *                 the preference name
   * @param {String} aType
   *                 either "boolean", "string" or "integer"
   *
   * @return {Boolean | String | Number} the requested preference
   */
  get: function TUP_get(aPref, aType)
  {
    if (!aPref || !aType) {
      return;
    }

    try {
      let prefs = this._branch;

      switch(aType) {
        case "boolean":
          return prefs.getBoolPref(aPref);
        case "string":
          return prefs.getCharPref(aPref);
        case "integer":
          return prefs.getIntPref(aPref);
      }
      return null;

    } catch(e) {
      // handle any unexpected exceptions
      TiltUtils.Output.error(e.message);
      return undefined;
    }
  },

  /**
   * Sets a custom Tilt preference.
   * If the preference already exists, it is overwritten.
   *
   * @param {String} aPref
   *                 the preference name
   * @param {String} aType
   *                 either "boolean", "string" or "integer"
   * @param {String} aValue
   *                 a new preference value
   *
   * @return {Boolean} true if the preference was set successfully
   */
  set: function TUP_set(aPref, aType, aValue)
  {
    if (!aPref || !aType || aValue === undefined || aValue === null) {
      return;
    }

    try {
      let prefs = this._branch;

      switch(aType) {
        case "boolean":
          return prefs.setBoolPref(aPref, aValue);
        case "string":
          return prefs.setCharPref(aPref, aValue);
        case "integer":
          return prefs.setIntPref(aPref, aValue);
      }
    } catch(e) {
      // handle any unexpected exceptions
      TiltUtils.Output.error(e.message);
    }
    return false;
  },

  /**
   * Creates a custom Tilt preference.
   * If the preference already exists, it is left unchanged.
   *
   * @param {String} aPref
   *                 the preference name
   * @param {String} aType
   *                 either "boolean", "string" or "integer"
   * @param {String} aValue
   *                 the initial preference value
   *
   * @return {Boolean} true if the preference was initialized successfully
   */
  create: function TUP_create(aPref, aType, aValue)
  {
    if (!aPref || !aType || aValue === undefined || aValue === null) {
      return;
    }

    try {
      let prefs = this._branch;

      if (!prefs.prefHasUserValue(aPref)) {
        switch(aType) {
          case "boolean":
            return prefs.setBoolPref(aPref, aValue);
          case "string":
            return prefs.setCharPref(aPref, aValue);
          case "integer":
            return prefs.setIntPref(aPref, aValue);
        }
      }
    } catch(e) {
      // handle any unexpected exceptions
      TiltUtils.Output.error(e.message);
    }
    return false;
  },

  /**
   * The preferences branch for this extension.
   */
  _branch: (function(aBranch) {
    return Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefService)
      .getBranch(aBranch);

  }("devtools.tilt."))
};

/**
 * Easy way to access the string bundle.
 */
TiltUtils.L10n = {

  /**
   * The string bundle element.
   */
  stringBundle: null,

  /**
   * Returns a string in the string bundle.
   * If the string bundle is not found, null is returned.
   *
   * @param {String} aName
   *                 the string name in the bundle
   *
   * @return {String} the equivalent string from the bundle
   */
  get: function TUL_get(aName)
  {
    // check to see if the parent string bundle document element is valid
    if (!this.stringBundle || !aName) {
      return null;
    }
    return this.stringBundle.GetStringFromName(aName);
  },

  /**
   * Returns a formatted string using the string bundle.
   * If the string bundle is not found, null is returned.
   *
   * @param {String} aName
   *                 the string name in the bundle
   * @param {Array} aArgs
   *                an array of arguments for the formatted string
   *
   * @return {String} the equivalent formatted string from the bundle
   */
  format: function TUL_format(aName, aArgs)
  {
    // check to see if the parent string bundle document element is valid
    if (!this.stringBundle || !aName || !aArgs) {
      return null;
    }
    return this.stringBundle.formatStringFromName(aName, aArgs, aArgs.length);
  }
};

/**
 * Utilities for accessing and manipulating a document.
 */
TiltUtils.DOM = {

  /**
   * Current parent node object used when creating canvas elements.
   */
  parentNode: null,

  /**
   * Helper method, allowing to easily create and manage a canvas element.
   * If the width and height params are falsy, they default to the parent node
   * client width and height.
   *
   * @param {Document} aParentNode
   *                   the parent node used to create the canvas
   *                   if not specified, it will be reused from the cache
   * @param {Object} aProperties
   *                 optional, object containing some of the following props:
   *       {Boolean} focusable
   *                 optional, true to make the canvas focusable
   *       {Boolean} append
   *                 optional, true to append the canvas to the parent node
   *        {Number} width
   *                 optional, specifies the width of the canvas
   *        {Number} height
   *                 optional, specifies the height of the canvas
   *        {String} id
   *                 optional, id for the created canvas element
   *
   * @return {HTMLCanvasElement} the newly created canvas element
   */
  initCanvas: function TUD_initCanvas(aParentNode, aProperties)
  {
    // check to see if the parent node element is valid
    if (!(aParentNode = aParentNode || this.parentNode)) {
      return null;
    }

    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // cache this parent node so that it can be reused
    this.parentNode = aParentNode;

    // create the canvas element
    let canvas = aParentNode.ownerDocument.
      createElementNS("http://www.w3.org/1999/xhtml", "canvas");

    let width = aProperties.width || aParentNode.clientWidth;
    let height = aProperties.height || aParentNode.clientHeight;
    let id = aProperties.id || null;

    canvas.setAttribute("style", "min-width: 1px; min-height: 1px;");
    canvas.setAttribute("width", width);
    canvas.setAttribute("height", height);
    canvas.setAttribute("id", id);

    // the canvas is unfocusable by default, we may require otherwise
    if (aProperties.focusable) {
      canvas.setAttribute("tabindex", "1");
      canvas.style.outline = "none";
    }

    // append the canvas element to the current parent node, if specified
    if (aProperties.append) {
      aParentNode.appendChild(canvas);
    }

    return canvas;
  },

  /**
   * Gets the full webpage dimensions (width and height).
   *
   * @param {Window} aContentWindow
   *                 the content window holding the document
   *
   * @return {Object} an object containing the width and height coords
   */
  getContentWindowDimensions: function TUD_getContentWindowDimensions(
    aContentWindow)
  {
    return {
      width: aContentWindow.innerWidth + aContentWindow.scrollMaxX,
      height: aContentWindow.innerHeight + aContentWindow.scrollMaxY
    };
  },

  /**
   * Calculates the position and depth to display a node, this can be overriden
   * to change the visualization.
   *
   * @param {Window} aContentWindow
   *                 the window content holding the document
   * @param {Node}   aNode
   *                 the node to get the position for
   * @param {Object} aParentPosition
   *                 the position of the parent node, as returned by this
   *                 function
   *
   * @return {Object} an object describing the node's position in 3D space
   *                  containing the following properties:
   *         {Number} top
   *                  distance along the x axis
   *         {Number} left
   *                  distance along the y axis
   *         {Number} depth
   *                  distance along the z axis
   *         {Number} width
   *                  width of the node
   *         {Number} height
   *                  height of the node
   *         {Number} thickness
   *                  thickness of the node
   */
  getNodePosition: function TUD_getNodePosition(aContentWindow, aNode,
                                                aParentPosition) {
    // get the x, y, width and height coordinates of the node
    let coord = LayoutHelpers.getRect(aNode, aContentWindow);
    if (!coord) {
      return null;
    }

    coord.depth = aParentPosition ? (aParentPosition.depth + aParentPosition.thickness) : 0;
    coord.thickness = STACK_THICKNESS;

    return coord;
  },

  /**
   * Traverses a document object model & calculates useful info for each node.
   *
   * @param {Window} aContentWindow
   *                 the window content holding the document
   * @param {Object} aProperties
   *                 optional, an object containing the following properties:
   *        {Object} invisibleElements
   *                 elements which should be ignored
   *        {Number} minSize
   *                 the minimum dimensions needed for a node to be traversed
   *        {Number} maxX
   *                 the maximum left position of an element
   *        {Number} maxY
   *                 the maximum top position of an element
   *
   * @return {Array} list containing nodes positions and local names
   */
  traverse: function TUD_traverse(aContentWindow, aProperties)
  {
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    let aInvisibleElements = aProperties.invisibleElements || {};
    let aMinSize = aProperties.minSize || -1;
    let aMaxX = aProperties.maxX || Number.MAX_VALUE;
    let aMaxY = aProperties.maxY || Number.MAX_VALUE;

    let nodes = aContentWindow.document.childNodes;
    let store = { info: [], nodes: [] };
    let depth = 0;

    let queue = [
      { parentPosition: null, nodes: aContentWindow.document.childNodes }
    ]

    while (queue.length) {
      let { nodes, parentPosition } = queue.shift();

      for (let node of nodes) {
        // skip some nodes to avoid visualization meshes that are too bloated
        let name = node.localName;
        if (!name || aInvisibleElements[name]) {
          continue;
        }

        let coord = this.getNodePosition(aContentWindow, node, parentPosition);
        if (!coord) {
          continue;
        }

        // the maximum size slices the traversal where needed
        if (coord.left > aMaxX || coord.top > aMaxY) {
          continue;
        }

        // use this node only if it actually has visible dimensions
        if (coord.width > aMinSize && coord.height > aMinSize) {

          // save the necessary details into a list to be returned later
          store.info.push({ coord: coord, name: name });
          store.nodes.push(node);
        }

        let childNodes = (name === "iframe" || name === "frame") ? node.contentDocument.childNodes : node.childNodes;
        if (childNodes.length > 0)
          queue.push({ parentPosition: coord, nodes: childNodes });
      }
    }

    return store;
  }
};

/**
 * Binds a new owner object to the child functions.
 * If the new parent is not specified, it will default to the passed scope.
 *
 * @param {Object} aScope
 *                 the object from which all functions will be rebound
 * @param {String} aRegex
 *                 a regular expression to identify certain functions
 * @param {Object} aParent
 *                 the new parent for the object's functions
 */
TiltUtils.bindObjectFunc = function TU_bindObjectFunc(aScope, aRegex, aParent)
{
  if (!aScope) {
    return;
  }

  for (let i in aScope) {
    try {
      if ("function" === typeof aScope[i] && (aRegex ? i.match(aRegex) : 1)) {
        aScope[i] = aScope[i].bind(aParent || aScope);
      }
    } catch(e) {
      TiltUtils.Output.error(e);
    }
  }
};

/**
 * Destroys an object and deletes all members.
 *
 * @param {Object} aScope
 *                 the object from which all children will be destroyed
 */
TiltUtils.destroyObject = function TU_destroyObject(aScope)
{
  if (!aScope) {
    return;
  }

  // objects in Tilt usually use a function to handle internal destruction
  if ("function" === typeof aScope._finalize) {
    aScope._finalize();
  }
  for (let i in aScope) {
    if (aScope.hasOwnProperty(i)) {
      delete aScope[i];
    }
  }
};

/**
 * Retrieve the unique ID of a window object.
 *
 * @param {Window} aWindow
 *                 the window to get the ID from
 *
 * @return {Number} the window ID
 */
TiltUtils.getWindowId = function TU_getWindowId(aWindow)
{
  if (!aWindow) {
    return;
  }

  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils)
                .currentInnerWindowID;
};

/**
 * Sets the markup document viewer zoom for the currently selected browser.
 *
 * @param {Window} aChromeWindow
 *                 the top-level browser window
 *
 * @param {Number} the zoom ammount
 */
TiltUtils.setDocumentZoom = function TU_setDocumentZoom(aChromeWindow, aZoom) {
  aChromeWindow.gBrowser.selectedBrowser.markupDocumentViewer.fullZoom = aZoom;
};

/**
 * Performs a garbage collection.
 *
 * @param {Window} aChromeWindow
 *                 the top-level browser window
 */
TiltUtils.gc = function TU_gc(aChromeWindow)
{
  aChromeWindow.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .garbageCollect();
};

/**
 * Clears the cache and sets all the variables to null.
 */
TiltUtils.clearCache = function TU_clearCache()
{
  TiltUtils.DOM.parentNode = null;
};

// bind the owner object to the necessary functions
TiltUtils.bindObjectFunc(TiltUtils.Output);
TiltUtils.bindObjectFunc(TiltUtils.Preferences);
TiltUtils.bindObjectFunc(TiltUtils.L10n);
TiltUtils.bindObjectFunc(TiltUtils.DOM);

// set the necessary string bundle
XPCOMUtils.defineLazyGetter(TiltUtils.L10n, "stringBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/devtools/tilt.properties");
});
