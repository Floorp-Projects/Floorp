/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The widget module currently supports only Firefox.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=560716
module.metadata = {
  "stability": "stable",
  "engines": {
    "Firefox": "*"
  }
};

// Widget content types
const CONTENT_TYPE_URI    = 1;
const CONTENT_TYPE_HTML   = 2;
const CONTENT_TYPE_IMAGE  = 3;

const ERR_CONTENT = "No content or contentURL property found. Widgets must "
                         + "have one or the other.",
      ERR_LABEL = "The widget must have a non-empty label property.",
      ERR_ID = "You have to specify a unique value for the id property of " +
               "your widget in order for the application to remember its " +
               "position.",
      ERR_DESTROYED = "The widget has been destroyed and can no longer be used.";

// Supported events, mapping from DOM event names to our event names
const EVENTS = {
  "click": "click",
  "mouseover": "mouseover",
  "mouseout": "mouseout",
};

const { validateOptions } = require("./deprecated/api-utils");
const panels = require("./panel");
const { EventEmitter, EventEmitterTrait } = require("./deprecated/events");
const { Trait } = require("./deprecated/traits");
const LightTrait = require('./deprecated/light-traits').Trait;
const { Loader, Symbiont } = require("./content/content");
const { Cortex } = require('./deprecated/cortex');
const windowsAPI = require("./windows");
const { WindowTracker } = require("./deprecated/window-utils");
const { isBrowser } = require("./window/utils");
const { setTimeout } = require("./timers");
const unload = require("./system/unload");
const { uuid } = require("./util/uuid");
const { getNodeView } = require("./view/core");

// Data types definition
const valid = {
  number: { is: ["null", "undefined", "number"] },
  string: { is: ["null", "undefined", "string"] },
  id: {
    is: ["string"],
    ok: function (v) v.length > 0,
    msg: ERR_ID,
    readonly: true
  },
  label: {
    is: ["string"],
    ok: function (v) v.length > 0,
    msg: ERR_LABEL
  },
  panel: {
    is: ["null", "undefined", "object"],
    ok: function(v) !v || v instanceof panels.Panel
  },
  width: {
    is: ["null", "undefined", "number"],
    map: function (v) {
      if (null === v || undefined === v) v = 16;
      return v;
    },
    defaultValue: 16
  },
  allow: {
    is: ["null", "undefined", "object"],
    map: function (v) {
      if (!v) v = { script: true };
      return v;
    },
    get defaultValue() ({ script: true })
  },
};

// Widgets attributes definition
let widgetAttributes = {
  label: valid.label,
  id: valid.id,
  tooltip: valid.string,
  width: valid.width,
  content: valid.string,
  panel: valid.panel,
  allow: valid.allow
};

// Import data definitions from loader, but don't compose with it as Model
// functions allow us to recreate easily all Loader code.
let loaderAttributes = require("./content/loader").validationAttributes;
for (let i in loaderAttributes)
  widgetAttributes[i] = loaderAttributes[i];

widgetAttributes.contentURL.optional = true;

// Widgets public events list, that are automatically binded in options object
const WIDGET_EVENTS = [
  "click",
  "mouseover",
  "mouseout",
  "error",
  "message",
  "attach"
];

// `Model` utility functions that help creating these various Widgets objects
let model = {

  // Validate one attribute using api-utils.js:validateOptions function
  _validate: function _validate(name, suspect, validation) {
    let $1 = {};
    $1[name] = suspect;
    let $2 = {};
    $2[name] = validation;
    return validateOptions($1, $2)[name];
  },

  /**
   * This method has two purposes:
   * 1/ Validate and define, on a given object, a set of attribute
   * 2/ Emit a "change" event on this object when an attribute is changed
   *
   * @params {Object} object
   *    Object on which we can bind attributes on and watch for their changes.
   *    This object must have an EventEmitter interface, or, at least `_emit`
   *    method
   * @params {Object} attrs
   *    Dictionary of attributes definition following api-utils:validateOptions
   *    scheme
   * @params {Object} values
   *    Dictionary of attributes default values
   */
  setAttributes: function setAttributes(object, attrs, values) {
    let properties = {};
    for (let name in attrs) {
      let value = values[name];
      let req = attrs[name];

      // Retrieve default value from typedef if the value is not defined
      if ((typeof value == "undefined" || value == null) && req.defaultValue)
        value = req.defaultValue;

      // Check for valid value if value is defined or mandatory
      if (!req.optional || typeof value != "undefined")
        value = model._validate(name, value, req);

      // In any case, define this property on `object`
      let property = null;
      if (req.readonly) {
        property = {
          value: value,
          writable: false,
          enumerable: true,
          configurable: false
        };
      }
      else {
        property = model._createWritableProperty(name, value);
      }

      properties[name] = property;
    }
    Object.defineProperties(object, properties);
  },

  // Generate ES5 property definition for a given attribute
  _createWritableProperty: function _createWritableProperty(name, value) {
    return {
      get: function () {
        return value;
      },
      set: function (newValue) {
        value = newValue;
        // The main goal of all this Model stuff is here:
        // We want to forward all changes to some listeners
        this._emit("change", name, value);
      },
      enumerable: true,
      configurable: false
    };
  },

  /**
   * Automagically register listeners in options dictionary
   * by detecting listener attributes with name starting with `on`
   *
   * @params {Object} object
   *    Target object that need to follow EventEmitter interface, or, at least,
   *    having `on` method.
   * @params {Array} events
   *    List of events name to automatically bind.
   * @params {Object} listeners
   *    Dictionary of event listener functions to register.
   */
  setEvents: function setEvents(object, events, listeners) {
    for (let i = 0, l = events.length; i < l; i++) {
      let name = events[i];
      let onName = "on" + name[0].toUpperCase() + name.substr(1);
      if (!listeners[onName])
        continue;
      object.on(name, listeners[onName].bind(object));
    }
  }

};


/**
 * Main Widget class: entry point of the widget API
 *
 * Allow to control all widget across all existing windows with a single object.
 * Widget.getView allow to retrieve a WidgetView instance to control a widget
 * specific to one window.
 */
const WidgetTrait = LightTrait.compose(EventEmitterTrait, LightTrait({

  _initWidget: function _initWidget(options) {
    model.setAttributes(this, widgetAttributes, options);

    browserManager.validate(this);

    // We must have at least content or contentURL defined
    if (!(this.content || this.contentURL))
      throw new Error(ERR_CONTENT);

    this._views = [];

    // Set tooltip to label value if we don't have tooltip defined
    if (!this.tooltip)
      this.tooltip = this.label;

    model.setEvents(this, WIDGET_EVENTS, options);

    this.on('change', this._onChange.bind(this));

    let self = this;
    this._port = EventEmitterTrait.create({
      emit: function () {
        let args = arguments;
        self._views.forEach(function(v) v.port.emit.apply(v.port, args));
      }
    });
    // expose wrapped port, that exposes only public properties.
    this._port._public = Cortex(this._port);

    // Register this widget to browser manager in order to create new widget on
    // all new windows
    browserManager.addItem(this);
  },

  _onChange: function _onChange(name, value) {
    // Set tooltip to label value if we don't have tooltip defined
    if (name == 'tooltip' && !value) {
      // we need to change tooltip again in order to change the value of the
      // attribute itself
      this.tooltip = this.label;
      return;
    }

    // Forward attributes changes to WidgetViews
    if (['width', 'tooltip', 'content', 'contentURL'].indexOf(name) != -1) {
      this._views.forEach(function(v) v[name] = value);
    }
  },

  _onEvent: function _onEvent(type, eventData) {
    this._emit(type, eventData);
  },

  _createView: function _createView() {
    // Create a new WidgetView instance
    let view = WidgetView(this);

    // Keep a reference to it
    this._views.push(view);

    return view;
  },

  // a WidgetView instance is destroyed
  _onViewDestroyed: function _onViewDestroyed(view) {
    let idx = this._views.indexOf(view);
    this._views.splice(idx, 1);
  },

  /**
   * Called on browser window closed, to destroy related WidgetViews
   * @params {ChromeWindow} window
   *         Window that has been closed
   */
  _onWindowClosed: function _onWindowClosed(window) {
    for each (let view in this._views) {
      if (view._isInChromeWindow(window)) {
        view.destroy();
        break;
      }
    }
  },

  /**
   * Get the WidgetView instance related to a BrowserWindow instance
   * @params {BrowserWindow} window
   *         BrowserWindow reference from "windows" module
   */
  getView: function getView(window) {
    for each (let view in this._views) {
      if (view._isInWindow(window)) {
        return view._public;
      }
    }
    return null;
  },

  get port() this._port._public,
  set port(v) {}, // Work around Cortex failure with getter without setter
                  // See bug 653464
  _port: null,

  postMessage: function postMessage(message) {
    this._views.forEach(function(v) v.postMessage(message));
  },

  destroy: function destroy() {
    if (this.panel)
      this.panel.destroy();

    // Dispatch destroy calls to views
    // we need to go backward as we remove items from this array in
    // _onViewDestroyed
    for (let i = this._views.length - 1; i >= 0; i--)
      this._views[i].destroy();

    // Unregister widget to stop creating it over new windows
    // and allow creation of new widget with same id
    browserManager.removeItem(this);
  }

}));

// Widget constructor
const Widget = function Widget(options) {
  let w = WidgetTrait.create(Widget.prototype);
  w._initWidget(options);

  // Return a Cortex of widget in order to hide private attributes like _onEvent
  let _public = Cortex(w);
  unload.ensure(_public, "destroy");
  return _public;
}
exports.Widget = Widget;


/**
 * WidgetView is an instance of a widget for a specific window.
 *
 * This is an external API that can be retrieved by calling Widget.getView or
 * by watching `attach` event on Widget.
 */
const WidgetViewTrait = LightTrait.compose(EventEmitterTrait, LightTrait({

  // Reference to the matching WidgetChrome
  // set right after constructor call
  _chrome: null,

  // Public interface of the WidgetView, passed in `attach` event or in
  // Widget.getView
  _public: null,

  _initWidgetView: function WidgetView__initWidgetView(baseWidget) {
    this._baseWidget = baseWidget;

    model.setAttributes(this, widgetAttributes, baseWidget);

    this.on('change', this._onChange.bind(this));

    let self = this;
    this._port = EventEmitterTrait.create({
      emit: function () {
        if (!self._chrome)
          throw new Error(ERR_DESTROYED);
        self._chrome.update(self._baseWidget, "emit", arguments);
      }
    });
    // expose wrapped port, that exposes only public properties.
    this._port._public = Cortex(this._port);

    this._public = Cortex(this);
  },

  // Called by WidgetChrome, when the related Worker is applied to the document,
  // so that we can start sending events to it
  _onWorkerReady: function () {
    // Emit an `attach` event with a WidgetView instance without private attrs
    this._baseWidget._emit("attach", this._public);
  },

  _onChange: function WidgetView__onChange(name, value) {
    if (name == 'tooltip' && !value) {
      this.tooltip = this.label;
      return;
    }

    // Forward attributes changes to WidgetChrome instance
    if (['width', 'tooltip', 'content', 'contentURL'].indexOf(name) != -1) {
      this._chrome.update(this._baseWidget, name, value);
    }
  },

  _onEvent: function WidgetView__onEvent(type, eventData, domNode) {
    // Dispatch event in view
    this._emit(type, eventData);

    // And forward it to the main Widget object
    if ("click" == type || type.indexOf("mouse") == 0)
      this._baseWidget._onEvent(type, this._public);
    else
      this._baseWidget._onEvent(type, eventData);

    // Special case for click events: if the widget doesn't have a click
    // handler, but it does have a panel, display the panel.
    if ("click" == type && !this._listeners("click").length && this.panel)
      // This kind of ugly workaround, instead we should implement
      // `getNodeView` for the `Widget` class itself, but that's kind of
      // hard without cleaning things up.
      this.panel.show(getNodeView.implement({}, function() domNode));
  },

  _isInWindow: function WidgetView__isInWindow(window) {
    return windowsAPI.BrowserWindow({
      window: this._chrome.window
    }) == window;
  },

  _isInChromeWindow: function WidgetView__isInChromeWindow(window) {
    return this._chrome.window == window;
  },

  _onPortEvent: function WidgetView__onPortEvent(args) {
    let port = this._port;
    port._emit.apply(port, args);
    let basePort = this._baseWidget._port;
    basePort._emit.apply(basePort, args);
  },

  get port() this._port._public,
  set port(v) {}, // Work around Cortex failure with getter without setter
                  // See bug 653464
  _port: null,

  postMessage: function WidgetView_postMessage(message) {
    if (!this._chrome)
      throw new Error(ERR_DESTROYED);
    this._chrome.update(this._baseWidget, "postMessage", message);
  },

  destroy: function WidgetView_destroy() {
    this._chrome.destroy();
    delete this._chrome;
    this._baseWidget._onViewDestroyed(this);
    this._emit("detach");
  }

}));


const WidgetView = function WidgetView(baseWidget) {
  let w = WidgetViewTrait.create(WidgetView.prototype);
  w._initWidgetView(baseWidget);
  return w;
}


/**
 * Keeps track of all browser windows.
 * Exposes methods for adding/removing widgets
 * across all open windows (and future ones).
 * Create a new instance of BrowserWindow per window.
 */
let browserManager = {
  items: [],
  windows: [],

  // Registers the manager to listen for window openings and closings.  Note
  // that calling this method can cause onTrack to be called immediately if
  // there are open windows.
  init: function () {
    let windowTracker = new WindowTracker(this);
    unload.ensure(windowTracker);
  },

  // Registers a window with the manager.  This is a WindowTracker callback.
  onTrack: function browserManager_onTrack(window) {
    if (isBrowser(window)) {
      let win = new BrowserWindow(window);
      win.addItems(this.items);
      this.windows.push(win);
    }
  },

  // Unregisters a window from the manager.  It's told to undo all
  // modifications.  This is a WindowTracker callback.  Note that when
  // WindowTracker is unloaded, it calls onUntrack for every currently opened
  // window.  The browserManager therefore doesn't need to specially handle
  // unload itself, since unloading the browserManager means untracking all
  // currently opened windows.
  onUntrack: function browserManager_onUntrack(window) {
    if (isBrowser(window)) {
      this.items.forEach(function(i) i._onWindowClosed(window));
      for (let i = 0; i < this.windows.length; i++) {
        if (this.windows[i].window == window) {
          this.windows.splice(i, 1)[0];
          return;
        }
      }

    }
  },

  // Used to validate widget by browserManager before adding it,
  // in order to check input very early in widget constructor
  validate : function (item) {
    let idx = this.items.indexOf(item);
    if (idx > -1)
      throw new Error("The widget " + item + " has already been added.");
    if (item.id) {
      let sameId = this.items.filter(function(i) i.id == item.id);
      if (sameId.length > 0)
        throw new Error("This widget ID is already used: " + item.id);
    } else {
      item.id = this.items.length;
    }
  },

  // Registers an item with the manager. It's added to all currently registered
  // windows, and when new windows are registered it will be added to them, too.
  addItem: function browserManager_addItem(item) {
    this.items.push(item);
    this.windows.forEach(function (w) w.addItems([item]));
  },

  // Unregisters an item from the manager.  It's removed from all windows that
  // are currently registered.
  removeItem: function browserManager_removeItem(item) {
    let idx = this.items.indexOf(item);
    if (idx > -1)
      this.items.splice(idx, 1);
  }
};



/**
 * Keeps track of a single browser window.
 *
 * This is where the core of how a widget's content is added to a window lives.
 */
function BrowserWindow(window) {
  this.window = window;
  this.doc = window.document;
}

BrowserWindow.prototype = {
  // Adds an array of items to the window.
  addItems: function BW_addItems(items) {
    items.forEach(this._addItemToWindow, this);
  },

  _addItemToWindow: function BW__addItemToWindow(baseWidget) {
    // Create a WidgetView instance
    let widget = baseWidget._createView();

    // Create a WidgetChrome instance
    let item = new WidgetChrome({
      widget: widget,
      doc: this.doc,
      window: this.window
    });

    widget._chrome = item;

    this._insertNodeInToolbar(item.node);

    // We need to insert Widget DOM Node before finishing widget view creation
    // (because fill creates an iframe and tries to access its docShell)
    item.fill();
  },

  _insertNodeInToolbar: function BW__insertNodeInToolbar(node) {
    // Add to the customization palette
    let toolbox = this.doc.getElementById("navigator-toolbox");
    let palette = toolbox.palette;
    palette.appendChild(node);

    // Search for widget toolbar by reading toolbar's currentset attribute
    let container = null;
    let toolbars = this.doc.getElementsByTagName("toolbar");
    let id = node.getAttribute("id");
    for (let i = 0, l = toolbars.length; i < l; i++) {
      let toolbar = toolbars[i];
      if (toolbar.getAttribute("currentset").indexOf(id) == -1)
        continue;
      container = toolbar;
    }

    // if widget isn't in any toolbar, add it to the addon-bar
    // TODO: we may want some "first-launch" module to do this only on very
    // first execution
    if (!container) {
      container = this.doc.getElementById("addon-bar");
      // TODO: find a way to make the following code work when we use "cfx run":
      // http://mxr.mozilla.org/mozilla-central/source/browser/base/content/browser.js#8586
      // until then, force display of addon bar directly from sdk code
      // https://bugzilla.mozilla.org/show_bug.cgi?id=627484
      if (container.collapsed)
        this.window.toggleAddonBar();
    }

    // Now retrieve a reference to the next toolbar item
    // by reading currentset attribute on the toolbar
    let nextNode = null;
    let currentSet = container.getAttribute("currentset");
    let ids = (currentSet == "__empty") ? [] : currentSet.split(",");
    let idx = ids.indexOf(id);
    if (idx != -1) {
      for (let i = idx; i < ids.length; i++) {
        nextNode = this.doc.getElementById(ids[i]);
        if (nextNode)
          break;
      }
    }

    // Finally insert our widget in the right toolbar and in the right position
    container.insertItem(id, nextNode, null, false);

    // Update DOM in order to save position: which toolbar, and which position
    // in this toolbar. But only do this the first time we add it to the toolbar
    // Otherwise, this code will collide with other instance of Widget module
    // during Firefox startup. See bug 685929.
    if (ids.indexOf(id) == -1) {
      container.setAttribute("currentset", container.currentSet);
      // Save DOM attribute in order to save position on new window opened
      this.window.document.persist(container.id, "currentset");
    }
  }
}


/**
 * Final Widget class that handles chrome DOM Node:
 *  - create initial DOM nodes
 *  - receive instruction from WidgetView through update method and update DOM
 *  - watch for DOM events and forward them to WidgetView
 */
function WidgetChrome(options) {
  this.window = options.window;
  this._doc = options.doc;
  this._widget = options.widget;
  this._symbiont = null; // set later
  this.node = null; // set later

  this._createNode();
}

// Update a property of a widget.
WidgetChrome.prototype.update = function WC_update(updatedItem, property, value) {
  switch(property) {
    case "contentURL":
    case "content":
      this.setContent();
      break;
    case "width":
      this.node.style.minWidth = value + "px";
      this.node.querySelector("iframe").style.width = value + "px";
      break;
    case "tooltip":
      this.node.setAttribute("tooltiptext", value);
      break;
    case "postMessage":
      this._symbiont.postMessage(value);
      break;
    case "emit":
      let port = this._symbiont.port;
      port.emit.apply(port, value);
      break;
  }
}

// Add a widget to this window.
WidgetChrome.prototype._createNode = function WC__createNode() {
  // XUL element container for widget
  let node = this._doc.createElement("toolbaritem");
  let guid = String(uuid());

  // Temporary work around require("self") failing on unit-test execution ...
  let jetpackID = "testID";
  try {
    jetpackID = require("./self").id;
  } catch(e) {}

  // Compute an unique and stable widget id with jetpack id and widget.id
  let id = "widget:" + jetpackID + "-" + this._widget.id;
  node.setAttribute("id", id);
  node.setAttribute("label", this._widget.label);
  node.setAttribute("tooltiptext", this._widget.tooltip);
  node.setAttribute("align", "center");
  // Bug 626326: Prevent customize toolbar context menu to appear
  node.setAttribute("context", "");

  // TODO move into a stylesheet, configurable by consumers.
  // Either widget.style, exposing the style object, or a URL
  // (eg, can load local stylesheet file).
  node.setAttribute("style", [
      "overflow: hidden; margin: 1px 2px 1px 2px; padding: 0px;",
      "min-height: 16px;",
  ].join(""));

  node.style.minWidth = this._widget.width + "px";

  this.node = node;
}

// Initial population of a widget's content.
WidgetChrome.prototype.fill = function WC_fill() {
  // Create element
  var iframe = this._doc.createElement("iframe");
  iframe.setAttribute("type", "content");
  iframe.setAttribute("transparent", "transparent");
  iframe.style.overflow = "hidden";
  iframe.style.height = "16px";
  iframe.style.maxHeight = "16px";
  iframe.style.width = this._widget.width + "px";
  iframe.setAttribute("flex", "1");
  iframe.style.border = "none";
  iframe.style.padding = "0px";

  // Do this early, because things like contentWindow are null
  // until the node is attached to a document.
  this.node.appendChild(iframe);

  // add event handlers
  this.addEventHandlers();

  // set content
  this.setContent();
}

// Get widget content type.
WidgetChrome.prototype.getContentType = function WC_getContentType() {
  if (this._widget.content)
    return CONTENT_TYPE_HTML;
  return (this._widget.contentURL && /\.(jpg|gif|png|ico|svg)$/i.test(this._widget.contentURL))
    ? CONTENT_TYPE_IMAGE : CONTENT_TYPE_URI;
}

// Set widget content.
WidgetChrome.prototype.setContent = function WC_setContent() {
  let type = this.getContentType();
  let contentURL = null;

  switch (type) {
    case CONTENT_TYPE_HTML:
      contentURL = "data:text/html;charset=utf-8," + encodeURIComponent(this._widget.content);
      break;
    case CONTENT_TYPE_URI:
      contentURL = this._widget.contentURL;
      break;
    case CONTENT_TYPE_IMAGE:
      let imageURL = this._widget.contentURL;
      contentURL = "data:text/html;charset=utf-8,<html><body><img src='" +
                   encodeURI(imageURL) + "'></body></html>";
      break;
    default:
      throw new Error("The widget's type cannot be determined.");
  }

  let iframe = this.node.firstElementChild;

  let self = this;
  // Cleanup previously created symbiont (in case we are update content)
  if (this._symbiont)
    this._symbiont.destroy();

  this._symbiont = Trait.compose(Symbiont.resolve({
    _onContentScriptEvent: "_onContentScriptEvent-not-used",
    _onInit: "_initSymbiont"
  }), {
    // Overload `Symbiont._onInit` in order to know when the related worker
    // is ready.
    _onInit: function () {
      this._initSymbiont();
      self._widget._onWorkerReady();
    },
    _onContentScriptEvent: function () {
      // Redirect events to WidgetView
      self._widget._onPortEvent(arguments);
    }
  })({
    frame: iframe,
    contentURL: contentURL,
    contentScriptFile: this._widget.contentScriptFile,
    contentScript: this._widget.contentScript,
    contentScriptWhen: this._widget.contentScriptWhen,
    contentScriptOptions: this._widget.contentScriptOptions,
    allow: this._widget.allow,
    onMessage: function(message) {
      setTimeout(function() {
        self._widget._onEvent("message", message);
      }, 0);
    }
  });
}

// Detect if document consists of a single image.
WidgetChrome._isImageDoc = function WC__isImageDoc(doc) {
  return /*doc.body &&*/ doc.body.childNodes.length == 1 &&
         doc.body.firstElementChild &&
         doc.body.firstElementChild.tagName == "IMG";
}

// Set up all supported events for a widget.
WidgetChrome.prototype.addEventHandlers = function WC_addEventHandlers() {
  let contentType = this.getContentType();

  let self = this;
  let listener = function(e) {
    // Ignore event firings that target the iframe.
    if (e.target == self.node.firstElementChild)
      return;

    // The widget only supports left-click for now,
    // so ignore all clicks (i.e. middle or right) except left ones.
    if (e.type == "click" && e.button !== 0)
      return;

    // Proxy event to the widget
    setTimeout(function() {
      self._widget._onEvent(EVENTS[e.type], null, self.node);
    }, 0);
  };

  this.eventListeners = {};
  let iframe = this.node.firstElementChild;
  for (let type in EVENTS) {
    iframe.addEventListener(type, listener, true, true);

    // Store listeners for later removal
    this.eventListeners[type] = listener;
  }

  // On document load, make modifications required for nice default
  // presentation.
  function loadListener(e) {
    let containerStyle = self.window.getComputedStyle(self.node.parentNode);
    // Ignore event firings that target the iframe
    if (e.target == iframe)
      return;
    // Ignore about:blank loads
    if (e.type == "load" && e.target.location == "about:blank")
      return;

    // We may have had an unload event before that cleaned up the symbiont
    if (!self._symbiont)
      self.setContent();

    let doc = e.target;

    if (contentType == CONTENT_TYPE_IMAGE || WidgetChrome._isImageDoc(doc)) {
      // Force image content to size.
      // Add-on authors must size their images correctly.
      doc.body.firstElementChild.style.width = self._widget.width + "px";
      doc.body.firstElementChild.style.height = "16px";
    }

    // Extend the add-on bar's default text styles to the widget.
    doc.body.style.color = containerStyle.color;
    doc.body.style.fontFamily = containerStyle.fontFamily;
    doc.body.style.fontSize = containerStyle.fontSize;
    doc.body.style.fontWeight = containerStyle.fontWeight;
    doc.body.style.textShadow = containerStyle.textShadow;
    // Allow all content to fill the box by default.
    doc.body.style.margin = "0";
  }

  iframe.addEventListener("load", loadListener, true);
  this.eventListeners["load"] = loadListener;

  // Register a listener to unload symbiont if the toolbaritem is moved
  // on user toolbars customization
  function unloadListener(e) {
    if (e.target.location == "about:blank")
      return;
    self._symbiont.destroy();
    self._symbiont = null;
    // This may fail but not always, it depends on how the node is
    // moved or removed
    try {
      self.setContent();
    } catch(e) {}

  }

  iframe.addEventListener("unload", unloadListener, true);
  this.eventListeners["unload"] = unloadListener;
}

// Remove and unregister the widget from everything
WidgetChrome.prototype.destroy = function WC_destroy(removedItems) {
  // remove event listeners
  for (let type in this.eventListeners) {
    let listener = this.eventListeners[type];
    this.node.firstElementChild.removeEventListener(type, listener, true);
  }
  // remove dom node
  this.node.parentNode.removeChild(this.node);
  // cleanup symbiont
  this._symbiont.destroy();
  // cleanup itself
  this.eventListeners = null;
  this._widget = null;
  this._symbiont = null;
}

// Init the browserManager only after setting prototypes and such above, because
// it will cause browserManager.onTrack to be called immediately if there are
// open windows.
browserManager.init();
