/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Cu, Ci} = require("chrome");
const promise = require("sdk/core/promise");
const IOService = Cc["@mozilla.org/network/io-service;1"]
  .getService(Ci.nsIIOService);
const {Spectrum} = require("devtools/shared/widgets/Spectrum");
const EventEmitter = require("devtools/shared/event-emitter");
const {colorUtils} = require("devtools/css-color");
const Heritage = require("sdk/core/heritage");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
  "resource:///modules/devtools/VariablesView.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "VariablesViewController",
  "resource:///modules/devtools/VariablesViewController.jsm");

const GRADIENT_RE = /\b(repeating-)?(linear|radial)-gradient\(((rgb|hsl)a?\(.+?\)|[^\)])+\)/gi;
const BORDERCOLOR_RE = /^border-[-a-z]*color$/ig;
const BORDER_RE = /^border(-(top|bottom|left|right))?$/ig;
const BACKGROUND_IMAGE_RE = /url\([\'\"]?(.*?)[\'\"]?\)/;
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const SPECTRUM_FRAME = "chrome://browser/content/devtools/spectrum-frame.xhtml";
const ESCAPE_KEYCODE = Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE;
const ENTER_KEYCODE = Ci.nsIDOMKeyEvent.DOM_VK_RETURN;

/**
 * Tooltip widget.
 *
 * This widget is intended at any tool that may need to show rich content in the
 * form of floating panels.
 * A common use case is image previewing in the CSS rule view, but more complex
 * use cases may include color pickers, object inspection, etc...
 *
 * Tooltips are based on XUL (namely XUL arrow-type <panel>s), and therefore
 * need a XUL Document to live in.
 * This is pretty much the only requirement they have on their environment.
 *
 * The way to use a tooltip is simply by instantiating a tooltip yourself and
 * attaching some content in it, or using one of the ready-made content types.
 *
 * A convenient `startTogglingOnHover` method may avoid having to register event
 * handlers yourself if the tooltip has to be shown when hovering over a
 * specific element or group of elements (which is usually the most common case)
 */

/**
 * Container used for dealing with optional parameters.
 *
 * @param {Object} defaults
 *        An object with all default options {p1: v1, p2: v2, ...}
 * @param {Object} options
 *        The actual values.
 */
function OptionsStore(defaults, options) {
  this.defaults = defaults || {};
  this.options = options || {};
}

OptionsStore.prototype = {
  /**
   * Get the value for a given option name.
   * @return {Object} Returns the value for that option, coming either for the
   *         actual values that have been set in the constructor, or from the
   *         defaults if that options was not specified.
   */
  get: function(name) {
    if (typeof this.options[name] !== "undefined") {
      return this.options[name];
    } else {
      return this.defaults[name];
    }
  }
};

/**
 * The low level structure of a tooltip is a XUL element (a <panel>).
 */
let PanelFactory = {
  /**
   * Get a new XUL panel instance.
   * @param {XULDocument} doc
   *        The XUL document to put that panel into
   * @param {OptionsStore} options
   *        An options store to get some configuration from
   */
  get: function(doc, options) {
    // Create the tooltip
    let panel = doc.createElement("panel");
    panel.setAttribute("hidden", true);
    panel.setAttribute("ignorekeys", true);

    // Prevent the click used to close the panel from being consumed
    panel.setAttribute("consumeoutsideclicks", options.get("consumeOutsideClick"));
    panel.setAttribute("noautofocus", options.get("noAutoFocus"));
    panel.setAttribute("type", "arrow");
    panel.setAttribute("level", "top");

    panel.setAttribute("class", "devtools-tooltip theme-tooltip-panel");
    doc.querySelector("window").appendChild(panel);

    return panel;
  }
};

/**
 * Tooltip class.
 *
 * Basic usage:
 *   let t = new Tooltip(xulDoc);
 *   t.content = someXulContent;
 *   t.show();
 *   t.hide();
 *   t.destroy();
 *
 * Better usage:
 *   let t = new Tooltip(xulDoc);
 *   t.startTogglingOnHover(container, target => {
 *     if (<condition based on target>) {
 *       t.setImageContent("http://image.png");
 *       return true;
 *     }
 *   });
 *   t.destroy();
 *
 * @param {XULDocument} doc
 *        The XUL document hosting this tooltip
 * @param {Object} options
 *        Optional options that give options to consumers
 *        - consumeOutsideClick {Boolean} Wether the first click outside of the
 *        tooltip should close the tooltip and be consumed or not.
 *        Defaults to false
 *        - closeOnKeys {Array} An array of key codes that should close the
 *        tooltip. Defaults to [27] (escape key)
 *        - noAutoFocus {Boolean} Should the focus automatically go to the panel
 *        when it opens. Defaults to true
 *
 * Fires these events:
 * - showing : just before the tooltip shows
 * - shown : when the tooltip is shown
 * - hiding : just before the tooltip closes
 * - hidden : when the tooltip gets hidden
 * - keypress : when any key gets pressed, with keyCode
 */
function Tooltip(doc, options) {
  EventEmitter.decorate(this);

  this.doc = doc;
  this.options = new OptionsStore({
    consumeOutsideClick: false,
    closeOnKeys: [ESCAPE_KEYCODE],
    noAutoFocus: true
  }, options);
  this.panel = PanelFactory.get(doc, this.options);

  // Used for namedTimeouts in the mouseover handling
  this.uid = "tooltip-" + Date.now();

  // Emit show/hide events
  for (let event of ["shown", "hidden", "showing", "hiding"]) {
    this["_onPopup" + event] = ((e) => {
      return () => this.emit(e);
    })(event);
    this.panel.addEventListener("popup" + event,
      this["_onPopup" + event], false);
  }

  // Listen to keypress events to close the tooltip if configured to do so
  let win = this.doc.querySelector("window");
  this._onKeyPress = event => {
    this.emit("keypress", event.keyCode);
    if (this.options.get("closeOnKeys").indexOf(event.keyCode) !== -1) {
      if (!this.panel.hidden) {
        event.stopPropagation();
      }
      this.hide();
    }
  };
  win.addEventListener("keypress", this._onKeyPress, false);
}

module.exports.Tooltip = Tooltip;

Tooltip.prototype = {
  defaultPosition: "before_start",
  defaultOffsetX: 0, // px
  defaultOffsetY: 0, // px
  defaultShowDelay: 50, // ms

  /**
   * Show the tooltip. It might be wise to append some content first if you
   * don't want the tooltip to be empty. You may access the content of the
   * tooltip by setting a XUL node to t.content.
   * @param {node} anchor
   *        Which node should the tooltip be shown on
   * @param {string} position [optional]
   *        Optional tooltip position. Defaults to before_start
   *        https://developer.mozilla.org/en-US/docs/XUL/PopupGuide/Positioning
   * @param {number} x, y [optional]
   *        The left and top offset coordinates, in pixels.
   */
  show: function(anchor,
    position = this.defaultPosition,
    x = this.defaultOffsetX,
    y = this.defaultOffsetY) {
    this.panel.hidden = false;
    this.panel.openPopup(anchor, position, x, y);
  },

  /**
   * Hide the tooltip
   */
  hide: function() {
    this.panel.hidden = true;
    this.panel.hidePopup();
  },

  isShown: function() {
    return this.panel.state !== "closed" && this.panel.state !== "hiding";
  },

  /**
   * Empty the tooltip's content
   */
  empty: function() {
    while (this.panel.hasChildNodes()) {
      this.panel.removeChild(this.panel.firstChild);
    }
  },

  /**
   * Gets this panel's visibility state.
   * @return boolean
   */
  isHidden: function() {
    return this.panel.state == "closed" || this.panel.state == "hiding";
  },

  /**
   * Gets if this panel has any child nodes.
   * @return boolean
   */
  isEmpty: function() {
    return !this.panel.hasChildNodes();
  },

  /**
   * Get rid of references and event listeners
   */
  destroy: function () {
    this.hide();

    for (let event of ["shown", "hidden", "showing", "hiding"]) {
      this.panel.removeEventListener("popup" + event,
        this["_onPopup" + event], false);
    }

    let win = this.doc.querySelector("window");
    win.removeEventListener("keypress", this._onKeyPress, false);

    this.content = null;

    if (this._basedNode) {
      this.stopTogglingOnHover();
    }

    this.doc = null;

    this.panel.remove();
    this.panel = null;
  },

  /**
   * Show/hide the tooltip when the mouse hovers over particular nodes.
   *
   * 2 Ways to make this work:
   * - Provide a single node to attach the tooltip to, as the baseNode, and
   *   omit the second targetNodeCb argument
   * - Provide a baseNode that is the container of possibly numerous children
   *   elements that may receive a tooltip. In this case, provide the second
   *   targetNodeCb argument to decide wether or not a child should receive
   *   a tooltip.
   *
   * This works by tracking mouse movements on a base container node (baseNode)
   * and showing the tooltip when the mouse stops moving. The targetNodeCb
   * callback is used to know whether or not the particular element being
   * hovered over should indeed receive the tooltip. If you don't provide it
   * it's equivalent to a function that always returns true.
   *
   * Note that if you call this function a second time, it will itself call
   * stopTogglingOnHover before adding mouse tracking listeners again.
   *
   * @param {node} baseNode
   *        The container for all target nodes
   * @param {Function} targetNodeCb
   *        A function that accepts a node argument and returns true or false
   *        to signify if the tooltip should be shown on that node or not.
   *        Additionally, the function receives a second argument which is the
   *        tooltip instance itself, to be used to add/modify the content of the
   *        tooltip if needed. If omitted, the tooltip will be shown everytime.
   * @param {Number} showDelay
   *        An optional delay that will be observed before showing the tooltip.
   *        Defaults to this.defaultShowDelay.
   */
  startTogglingOnHover: function(baseNode, targetNodeCb, showDelay = this.defaultShowDelay) {
    if (this._basedNode) {
      this.stopTogglingOnHover();
    }

    this._basedNode = baseNode;
    this._showDelay = showDelay;
    this._targetNodeCb = targetNodeCb || (() => true);

    this._onBaseNodeMouseMove = this._onBaseNodeMouseMove.bind(this);
    this._onBaseNodeMouseLeave = this._onBaseNodeMouseLeave.bind(this);

    baseNode.addEventListener("mousemove", this._onBaseNodeMouseMove, false);
    baseNode.addEventListener("mouseleave", this._onBaseNodeMouseLeave, false);
  },

  /**
   * If the startTogglingOnHover function has been used previously, and you want
   * to get rid of this behavior, then call this function to remove the mouse
   * movement tracking
   */
  stopTogglingOnHover: function() {
    clearNamedTimeout(this.uid);

    this._basedNode.removeEventListener("mousemove",
      this._onBaseNodeMouseMove, false);
    this._basedNode.removeEventListener("mouseleave",
      this._onBaseNodeMouseLeave, false);

    this._basedNode = null;
    this._targetNodeCb = null;
    this._lastHovered = null;
  },

  _onBaseNodeMouseMove: function(event) {
    if (event.target !== this._lastHovered) {
      this.hide();
      this._lastHovered = event.target;
      setNamedTimeout(this.uid, this._showDelay, () => {
        this._showOnHover(event.target);
      });
    }
  },

  _showOnHover: function(target) {
    if (this._targetNodeCb(target, this)) {
      this.show(target);
    }
  },

  _onBaseNodeMouseLeave: function() {
    clearNamedTimeout(this.uid);
    this._lastHovered = null;
    this.hide();
  },

  /**
   * Set the content of this tooltip. Will first empty the tooltip and then
   * append the new content element.
   * Consider using one of the set<type>Content() functions instead.
   * @param {node} content
   *        A node that can be appended in the tooltip XUL element
   */
  set content(content) {
    if (this.content == content) {
      return;
    }

    this.empty();
    this.panel.removeAttribute("clamped-dimensions");

    if (content) {
      this.panel.appendChild(content);
    }
  },

  get content() {
    return this.panel.firstChild;
  },

  /**
   * Sets some text as the content of this tooltip.
   *
   * @param {array} messages
   *        A list of text messages.
   * @param {string} messagesClass [optional]
   *        A style class for the text messages.
   * @param {string} containerClass [optional]
   *        A style class for the text messages container.
   */
  setTextContent: function(messages,
    messagesClass = "default-tooltip-simple-text-colors",
    containerClass = "default-tooltip-simple-text-colors") {

    let vbox = this.doc.createElement("vbox");
    vbox.className = "devtools-tooltip-simple-text-container " + containerClass;
    vbox.setAttribute("flex", "1");

    for (let text of messages) {
      let description = this.doc.createElement("description");
      description.setAttribute("flex", "1");
      description.className = "devtools-tooltip-simple-text " + messagesClass;
      description.textContent = text;
      vbox.appendChild(description);
    }

    this.content = vbox;
  },

  /**
   * Fill the tooltip with a variables view, inspecting an object via its
   * corresponding object actor, as specified in the remote debugging protocol.
   *
   * @param {object} objectActor
   *        The value grip for the object actor.
   * @param {object} viewOptions [optional]
   *        Options for the variables view visualization.
   * @param {object} controllerOptions [optional]
   *        Options for the variables view controller.
   * @param {object} relayEvents [optional]
   *        A collection of events to listen on the variables view widget.
   *        For example, { fetched: () => ... }
   * @param {boolean} reuseCachedWidget [optional]
   *        Pass false to instantiate a brand new widget for this variable.
   *        Otherwise, if a variable was previously inspected, its widget
   *        will be reused.
   */
  setVariableContent: function(
    objectActor,
    viewOptions = {},
    controllerOptions = {},
    relayEvents = {},
    reuseCachedWidget = true) {

    if (reuseCachedWidget && this._cachedVariablesView) {
      var [vbox, widget] = this._cachedVariablesView;
    } else {
      var vbox = this.doc.createElement("vbox");
      vbox.className = "devtools-tooltip-variables-view-box";
      vbox.setAttribute("flex", "1");

      let innerbox = this.doc.createElement("vbox");
      innerbox.className = "devtools-tooltip-variables-view-innerbox";
      innerbox.setAttribute("flex", "1");
      vbox.appendChild(innerbox);

      var widget = new VariablesView(innerbox, viewOptions);
      for (let e in relayEvents) widget.on(e, relayEvents[e]);
      VariablesViewController.attach(widget, controllerOptions);

      this._cachedVariablesView = [vbox, widget];
    }

    // Some of the view options are allowed to change between uses.
    widget.searchPlaceholder = viewOptions.searchPlaceholder;
    widget.searchEnabled = viewOptions.searchEnabled;

    // Use the object actor's grip to display it as a variable in the widget.
    // The controller options are allowed to change between uses.
    widget.controller.setSingleVariable(
      { objectActor: objectActor }, controllerOptions);

    this.content = vbox;
    this.panel.setAttribute("clamped-dimensions", "");
  },

  /**
   * Fill the tooltip with an image, displayed over a tiled background useful
   * for transparent images. Also adds the image dimension as a label at the
   * bottom.
   * @param {string} imageUrl
   *        The url to load the image from
   * @param {Object} options
   *        The following options are supported:
   *        - resized : whether or not the image identified by imageUrl has been
   *        resized before this function was called.
   *        - naturalWidth/naturalHeight : the original size of the image before
   *        it was resized, if if was resized before this function was called.
   *        If not provided, will be measured on the loaded image.
   *        - maxDim : if the image should be resized before being shown, pass
   *        a number here
   */
  setImageContent: function(imageUrl, options={}) {
    // Main container
    let vbox = this.doc.createElement("vbox");
    vbox.setAttribute("align", "center")

    // Display the image
    let image = this.doc.createElement("image");
    image.setAttribute("src", imageUrl);
    if (options.maxDim) {
      image.style.maxWidth = options.maxDim + "px";
      image.style.maxHeight = options.maxDim + "px";
    }
    vbox.appendChild(image);

    // Temporary label during image load
    let label = this.doc.createElement("label");
    label.classList.add("devtools-tooltip-caption");
    label.classList.add("theme-comment");
    label.textContent = l10n.strings.GetStringFromName("previewTooltip.image.brokenImage");
    vbox.appendChild(label);

    this.content = vbox;

    // Load the image to get dimensions and display it when done
    let imgObj = new this.doc.defaultView.Image();
    imgObj.src = imageUrl;
    imgObj.onload = () => {
      imgObj.onload = null;

      // Display dimensions
      let w = options.naturalWidth || imgObj.naturalWidth;
      let h = options.naturalHeight || imgObj.naturalHeight;
      label.textContent = w + " x " + h;
    }
  },

  /**
   * Exactly the same as the `image` function but takes a css background image
   * value instead : url(....)
   */
  setCssBackgroundImageContent: function(cssBackground, sheetHref, maxDim=400) {
    let uri = getBackgroundImageUri(cssBackground, sheetHref);
    if (uri) {
      this.setImageContent(uri, {
        maxDim: maxDim
      });
    }
  },

  /**
   * Fill the tooltip with a new instance of the spectrum color picker widget
   * initialized with the given color, and return a promise that resolves to
   * the instance of spectrum
   */
  setColorPickerContent: function(color) {
    let def = promise.defer();

    // Create an iframe to contain spectrum
    let iframe = this.doc.createElementNS(XHTML_NS, "iframe");
    iframe.setAttribute("transparent", true);
    iframe.setAttribute("width", "210");
    iframe.setAttribute("height", "195");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("class", "devtools-tooltip-iframe");

    let panel = this.panel;
    let xulWin = this.doc.ownerGlobal;

    // Wait for the load to initialize spectrum
    function onLoad() {
      iframe.removeEventListener("load", onLoad, true);
      let win = iframe.contentWindow.wrappedJSObject;

      let container = win.document.getElementById("spectrum");
      let spectrum = new Spectrum(container, color);

      // Finalize spectrum's init when the tooltip becomes visible
      panel.addEventListener("popupshown", function shown() {
        panel.removeEventListener("popupshown", shown, true);
        spectrum.show();
        def.resolve(spectrum);
      }, true);
    }
    iframe.addEventListener("load", onLoad, true);
    iframe.setAttribute("src", SPECTRUM_FRAME);

    // Put the iframe in the tooltip
    this.content = iframe;

    return def.promise;
  }
};

/**
 * Base class for all (color, gradient, ...)-swatch based value editors inside
 * tooltips
 *
 * @param {XULDocument} doc
 */
function SwatchBasedEditorTooltip(doc) {
  // Creating a tooltip instance
  // This one will consume outside clicks as it makes more sense to let the user
  // close the tooltip by clicking out
  // It will also close on <escape> and <enter>
  this.tooltip = new Tooltip(doc, {
    consumeOutsideClick: true,
    closeOnKeys: [ESCAPE_KEYCODE, ENTER_KEYCODE],
    noAutoFocus: false
  });

  // By default, swatch-based editor tooltips revert value change on <esc> and
  // commit value change on <enter>
  this._onTooltipKeypress = (event, code) => {
    if (code === ESCAPE_KEYCODE) {
      this.revert();
    } else if (code === ENTER_KEYCODE) {
      this.commit();
    }
  };
  this.tooltip.on("keypress", this._onTooltipKeypress);

  // All target swatches are kept in a map, indexed by swatch DOM elements
  this.swatches = new Map();

  // When a swatch is clicked, and for as long as the tooltip is shown, the
  // activeSwatch property will hold the reference to the swatch DOM element
  // that was clicked
  this.activeSwatch = null;

  this._onSwatchClick = this._onSwatchClick.bind(this);
}

SwatchBasedEditorTooltip.prototype = {
  show: function() {
    if (this.activeSwatch) {
      this.tooltip.show(this.activeSwatch, "topcenter bottomleft");
    }
  },

  hide: function() {
    this.tooltip.hide();
  },

  /**
   * Add a new swatch DOM element to the list of swatch elements this editor
   * tooltip knows about. That means from now on, clicking on that swatch will
   * toggle the editor.
   *
   * @param {node} swatchEl
   *        The element to add
   * @param {object} callbacks
   *        Callbacks that will be executed when the editor wants to preview a
   *        value change, or revert a change, or commit a change.
   * @param {object} originalValue
   *        The original value before the editor in the tooltip makes changes
   *        This can be of any type, and will be passed, as is, in the revert
   *        callback
   */
  addSwatch: function(swatchEl, callbacks={}, originalValue) {
    if (!callbacks.onPreview) callbacks.onPreview = function() {};
    if (!callbacks.onRevert) callbacks.onRevert = function() {};
    if (!callbacks.onCommit) callbacks.onCommit = function() {};

    this.swatches.set(swatchEl, {
      callbacks: callbacks,
      originalValue: originalValue
    });
    swatchEl.addEventListener("click", this._onSwatchClick, false);
  },

  removeSwatch: function(swatchEl) {
    if (this.swatches.has(swatchEl)) {
      if (this.activeSwatch === swatchEl) {
        this.hide();
        this.activeSwatch = null;
      }
      swatchEl.removeEventListener("click", this._onSwatchClick, false);
      this.swatches.delete(swatchEl);
    }
  },

  _onSwatchClick: function(event) {
    let swatch = this.swatches.get(event.target);
    if (swatch) {
      this.activeSwatch = event.target;
      this.show();
      event.stopPropagation();
    }
  },

  /**
   * Not called by this parent class, needs to be taken care of by sub-classes
   */
  preview: function(value) {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onPreview(value);
    }
  },

  /**
   * This parent class only calls this on <esc> keypress
   */
  revert: function() {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onRevert(swatch.originalValue);
    }
  },

  /**
   * This parent class only calls this on <enter> keypress
   */
  commit: function() {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onCommit();
    }
  },

  destroy: function() {
    this.swatches.clear();
    this.activeSwatch = null;
    this.tooltip.off("keypress", this._onTooltipKeypress);
    this.tooltip.destroy();
  }
};

/**
 * The swatch color picker tooltip class is a specific class meant to be used
 * along with output-parser's generated color swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * color picker.
 *
 * @param {XULDocument} doc
 */
function SwatchColorPickerTooltip(doc) {
  SwatchBasedEditorTooltip.call(this, doc);

  // Creating a spectrum instance. this.spectrum will always be a promise that
  // resolves to the spectrum instance
  this.spectrum = this.tooltip.setColorPickerContent([0, 0, 0, 1]);
  this._onSpectrumColorChange = this._onSpectrumColorChange.bind(this);
}

module.exports.SwatchColorPickerTooltip = SwatchColorPickerTooltip;

SwatchColorPickerTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set spectrum's
   * color.
   */
  show: function() {
    // Call then parent class' show function
    SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set spectrum's color and listen to color changes to preview them
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      let color = this.activeSwatch.style.backgroundColor;
      this.spectrum.then(spectrum => {
        spectrum.off("changed", this._onSpectrumColorChange);
        spectrum.rgb = this._colorToRgba(color);
        spectrum.on("changed", this._onSpectrumColorChange);
        spectrum.updateUI();
      });
    }
  },

  _onSpectrumColorChange: function(event, rgba, cssColor) {
    if (this.activeSwatch) {
      this.activeSwatch.style.backgroundColor = cssColor;
      this.activeSwatch.nextSibling.textContent = cssColor;
      this.preview(cssColor);
    }
  },

  _colorToRgba: function(color) {
    color = new colorUtils.CssColor(color);
    let rgba = color._getRGBATuple();
    return [rgba.r, rgba.g, rgba.b, rgba.a];
  },

  destroy: function() {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.spectrum.then(spectrum => {
      spectrum.off("changed", this._onSpectrumColorChange);
      spectrum.destroy();
    });
  }
});

/**
 * Internal util, checks whether a css declaration is a gradient
 */
function isGradientRule(property, value) {
  return (property === "background" || property === "background-image") &&
    value.match(GRADIENT_RE);
}

/**
 * Internal util, checks whether a css declaration is a color
 */
function isColorOnly(property, value) {
  return property === "background-color" ||
         property === "color" ||
         property.match(BORDERCOLOR_RE);
}

/**
 * Internal util, returns the background image uri if any
 */
function getBackgroundImageUri(value, sheetHref) {
  let uriMatch = BACKGROUND_IMAGE_RE.exec(value);
  let uri = null;

  if (uriMatch && uriMatch[1]) {
    uri = uriMatch[1];
    if (sheetHref) {
      let sheetUri = IOService.newURI(sheetHref, null, null);
      uri = sheetUri.resolve(uri);
    }
  }

  return uri;
}

/**
 * L10N utility class
 */
function L10N() {}
L10N.prototype = {};

let l10n = new L10N();

loader.lazyGetter(L10N.prototype, "strings", () => {
  return Services.strings.createBundle(
    "chrome://browser/locale/devtools/inspector.properties");
});
