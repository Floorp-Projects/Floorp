/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The panel module currently supports only Firefox.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=jetpack-panel-apps
module.metadata = {
  "stability": "stable",
  "engines": {
    "Firefox": "*"
  }
};

const { Ci } = require("chrome");
const { validateOptions: valid } = require('./deprecated/api-utils');
const { Symbiont } = require('./content/content');
const { EventEmitter } = require('./deprecated/events');
const { setTimeout } = require('./timers');
const { on, off, emit } = require('./system/events');
const runtime = require('./system/runtime');
const { getDocShell } = require("./frame/utils");
const { getWindow } = require('./panel/window');
const { isPrivateBrowsingSupported } = require('./self');
const { isWindowPBSupported } = require('./private-browsing/utils');
const { getNodeView } = require('./view/core');

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      ON_SHOW = 'popupshown',
      ON_HIDE = 'popuphidden',
      validNumber = { is: ['number', 'undefined', 'null'] },
      validBoolean = { is: ['boolean', 'undefined', 'null'] },
      ADDON_ID = require('./self').id;

if (isPrivateBrowsingSupported && isWindowPBSupported) {
  throw Error('The panel module cannot be used with per-window private browsing at the moment, see Bug 816257');
}

/**
 * Emits show and hide events.
 */
const Panel = Symbiont.resolve({
  constructor: '_init',
  _onInit: '_onSymbiontInit',
  destroy: '_symbiontDestructor',
  _documentUnload: '_workerDocumentUnload'
}).compose({
  _frame: Symbiont.required,
  _init: Symbiont.required,
  _onSymbiontInit: Symbiont.required,
  _symbiontDestructor: Symbiont.required,
  _emit: Symbiont.required,
  on: Symbiont.required,
  removeListener: Symbiont.required,

  _inited: false,

  /**
   * If set to `true` frame loaders between xul panel frame and
   * hidden frame are swapped. If set to `false` frame loaders are
   * set back to normal. Setting the value that was already set will
   * have no effect.
   */
  set _frameLoadersSwapped(value) {
    if (this.__frameLoadersSwapped == value) return;
    this._frame.QueryInterface(Ci.nsIFrameLoaderOwner)
      .swapFrameLoaders(this._viewFrame);
    this.__frameLoadersSwapped = value;
  },
  __frameLoadersSwapped: false,

  constructor: function Panel(options) {
    this._onShow = this._onShow.bind(this);
    this._onHide = this._onHide.bind(this);
    this._onAnyPanelShow = this._onAnyPanelShow.bind(this);
    on('sdk-panel-show', this._onAnyPanelShow);

    this.on('inited', this._onSymbiontInit.bind(this));
    this.on('propertyChange', this._onChange.bind(this));

    options = options || {};
    if ('onShow' in options)
      this.on('show', options.onShow);
    if ('onHide' in options)
      this.on('hide', options.onHide);
    if ('width' in options)
      this.width = options.width;
    if ('height' in options)
      this.height = options.height;
    if ('contentURL' in options)
      this.contentURL = options.contentURL;
    if ('focus' in options) {
      var value = options.focus;
      var validatedValue = valid({ $: value }, { $: validBoolean }).$;
      this._focus =
        (typeof validatedValue == 'boolean') ? validatedValue : this._focus;
    }

    this._init(options);
  },
  _destructor: function _destructor() {
    this.hide();
    this._removeAllListeners('show');
    this._removeAllListeners('hide');
    this._removeAllListeners('propertyChange');
    this._removeAllListeners('inited');
    off('sdk-panel-show', this._onAnyPanelShow);
    // defer cleanup to be performed after panel gets hidden
    this._xulPanel = null;
    this._symbiontDestructor(this);
    this._removeAllListeners();
  },
  destroy: function destroy() {
    this._destructor();
  },
  /* Public API: Panel.width */
  get width() this._width,
  set width(value)
    this._width = valid({ $: value }, { $: validNumber }).$ || this._width,
  _width: 320,
  /* Public API: Panel.height */
  get height() this._height,
  set height(value)
    this._height =  valid({ $: value }, { $: validNumber }).$ || this._height,
  _height: 240,
  /* Public API: Panel.focus */
  get focus() this._focus,
  _focus: true,

  /* Public API: Panel.isShowing */
  get isShowing() !!this._xulPanel && this._xulPanel.state == "open",

  /* Public API: Panel.show */
  show: function show(anchor) {
    anchor = anchor ? getNodeView(anchor) : null;
    let anchorWindow = getWindow(anchor);

    // If there is no open window, or the anchor is in a private window
    // then we will not be able to display the panel
    if (!anchorWindow) {
      return;
    }

    let document = anchorWindow.document;
    let xulPanel = this._xulPanel;
    let panel = this;
    if (!xulPanel) {
      xulPanel = this._xulPanel = document.createElementNS(XUL_NS, 'panel');
      xulPanel.setAttribute("type", "arrow");

      // One anonymous node has a big padding that doesn't work well with
      // Jetpack, as we would like to display an iframe that completely fills
      // the panel.
      // -> Use a XBL wrapper with inner stylesheet to remove this padding.
      let css = ".panel-inner-arrowcontent, .panel-arrowcontent {padding: 0;}";
      let originalXBL = "chrome://global/content/bindings/popup.xml#arrowpanel";
      let binding =
      '<bindings xmlns="http://www.mozilla.org/xbl">' +
        '<binding id="id" extends="' + originalXBL + '">' +
          '<resources>' +
            '<stylesheet src="data:text/css;charset=utf-8,' +
              document.defaultView.encodeURIComponent(css) + '"/>' +
          '</resources>' +
        '</binding>' +
      '</bindings>';
      xulPanel.style.MozBinding = 'url("data:text/xml;charset=utf-8,' +
        document.defaultView.encodeURIComponent(binding) + '")';

      let frame = document.createElementNS(XUL_NS, 'iframe');
      frame.setAttribute('type', 'content');
      frame.setAttribute('flex', '1');
      frame.setAttribute('transparent', 'transparent');

      if (runtime.OS === "Darwin") {
        frame.style.borderRadius = "6px";
        frame.style.padding = "1px";
      }

      // Load an empty document in order to have an immediatly loaded iframe,
      // so swapFrameLoaders is going to work without having to wait for load.
      frame.setAttribute("src","data:;charset=utf-8,");

      xulPanel.appendChild(frame);
      document.getElementById("mainPopupSet").appendChild(xulPanel);
    }
    let { width, height, focus } = this, x, y, position;

    if (!anchor) {
      // Open the popup in the middle of the window.
      x = document.documentElement.clientWidth / 2 - width / 2;
      y = document.documentElement.clientHeight / 2 - height / 2;
      position = null;
    }
    else {
      // Open the popup by the anchor.
      let rect = anchor.getBoundingClientRect();

      let window = anchor.ownerDocument.defaultView;

      let zoom = window.mozScreenPixelsPerCSSPixel;
      let screenX = rect.left + window.mozInnerScreenX * zoom;
      let screenY = rect.top + window.mozInnerScreenY * zoom;

      // Set up the vertical position of the popup relative to the anchor
      // (always display the arrow on anchor center)
      let horizontal, vertical;
      if (screenY > window.screen.availHeight / 2 + height)
        vertical = "top";
      else
        vertical = "bottom";

      if (screenY > window.screen.availWidth / 2 + width)
        horizontal = "left";
      else
        horizontal = "right";

      let verticalInverse = vertical == "top" ? "bottom" : "top";
      position = vertical + "center " + verticalInverse + horizontal;

      // Allow panel to flip itself if the panel can't be displayed at the
      // specified position (useful if we compute a bad position or if the
      // user moves the window and panel remains visible)
      xulPanel.setAttribute("flip","both");
    }

    // Resize the iframe instead of using panel.sizeTo
    // because sizeTo doesn't work with arrow panels
    xulPanel.firstChild.style.width = width + "px";
    xulPanel.firstChild.style.height = height + "px";

    // Only display xulPanel if Panel.hide() was not called
    // after Panel.show(), but before xulPanel.openPopup
    // was loaded
    emit('sdk-panel-show', { data: ADDON_ID, subject: xulPanel });

    // Prevent the panel from getting focus when showing up
    // if focus is set to false
    xulPanel.setAttribute("noautofocus",!focus);

    // Wait for the XBL binding to be constructed
    function waitForBinding() {
      if (!xulPanel.openPopup) {
        setTimeout(waitForBinding, 50);
        return;
      }

      if (xulPanel.state !== 'hiding') {
        xulPanel.openPopup(anchor, position, x, y);
      }
    }
    waitForBinding();

    return this._public;
  },
  /* Public API: Panel.hide */
  hide: function hide() {
    // The popuphiding handler takes care of swapping back the frame loaders
    // and removing the XUL panel from the application window, we just have to
    // trigger it by hiding the popup.
    // XXX Sometimes I get "TypeError: xulPanel.hidePopup is not a function"
    // when quitting the host application while a panel is visible.  To suppress
    // them, this now checks for "hidePopup" in xulPanel before calling it.
    // It's not clear if there's an actual issue or the error is just normal.
    let xulPanel = this._xulPanel;
    if (xulPanel && "hidePopup" in xulPanel)
      xulPanel.hidePopup();
    return this._public;
  },

  /* Public API: Panel.resize */
  resize: function resize(width, height) {
    this.width = width;
    this.height = height;
    // Resize the iframe instead of using panel.sizeTo
    // because sizeTo doesn't work with arrow panels
    let xulPanel = this._xulPanel;
    if (xulPanel) {
      xulPanel.firstChild.style.width = width + "px";
      xulPanel.firstChild.style.height = height + "px";
    }
  },

  // While the panel is visible, this is the XUL <panel> we use to display it.
  // Otherwise, it's null.
  get _xulPanel() this.__xulPanel,
  set _xulPanel(value) {
    let xulPanel = this.__xulPanel;
    if (value === xulPanel) return;
    if (xulPanel) {
      xulPanel.removeEventListener(ON_HIDE, this._onHide, false);
      xulPanel.removeEventListener(ON_SHOW, this._onShow, false);
      xulPanel.parentNode.removeChild(xulPanel);
    }
    if (value) {
      value.addEventListener(ON_HIDE, this._onHide, false);
      value.addEventListener(ON_SHOW, this._onShow, false);
    }
    this.__xulPanel = value;
  },
  __xulPanel: null,
  get _viewFrame() this.__xulPanel.children[0],
  /**
   * When the XUL panel becomes hidden, we swap frame loaders back to move
   * the content of the panel to the hidden frame & remove panel element.
   */
  _onHide: function _onHide() {
    try {
      this._frameLoadersSwapped = false;
      this._xulPanel = null;
      this._emit('hide');
    } catch(e) {
      this._emit('error', e);
    }
  },

  /**
   * Retrieve computed text color style in order to apply to the iframe
   * document. As MacOS background is dark gray, we need to use skin's
   * text color.
   */
  _applyStyleToDocument: function _applyStyleToDocument() {
    if (this._defaultStyleApplied)
      return;
    try {
      let win = this._xulPanel.ownerDocument.defaultView;
      let node = win.document.getAnonymousElementByAttribute(
        this._xulPanel, "class", "panel-arrowcontent");
      if (!node) {
        // Before bug 764755, anonymous content was different:
        // TODO: Remove this when targeting FF16+
        node = win.document.getAnonymousElementByAttribute(
          this._xulPanel, "class", "panel-inner-arrowcontent");
      }
      let textColor = win.getComputedStyle(node).getPropertyValue("color");
      let doc = this._xulPanel.firstChild.contentDocument;
      let style = doc.createElement("style");
      style.textContent = "body { color: " + textColor + "; }";
      let container = doc.head ? doc.head : doc.documentElement;

      if (container.firstChild)
        container.insertBefore(style, container.firstChild);
      else
        container.appendChild(style);
      this._defaultStyleApplied = true;
    }
    catch(e) {
      console.error("Unable to apply panel style");
      console.exception(e);
    }
  },

  /**
   * When the XUL panel becomes shown, we swap frame loaders between panel
   * frame and hidden frame to preserve state of the content dom.
   */
  _onShow: function _onShow() {
    try {
      if (!this._inited) { // defer if not initialized yet
        this.on('inited', this._onShow.bind(this));
      } else {
        this._frameLoadersSwapped = true;
        this._applyStyleToDocument();
        this._emit('show');
      }
    } catch(e) {
      this._emit('error', e);
    }
  },

  /**
   * When any panel is displayed, system-wide, close `this`
   * panel unless it's the most recently displayed panel
   */
  _onAnyPanelShow: function _onAnyPanelShow(e) {
    if (e.subject !== this._xulPanel)
      this.hide();
  },

  /**
   * Notification that panel was fully initialized.
   */
  _onInit: function _onInit() {
    this._inited = true;

    // Avoid panel document from resizing the browser window
    // New platform capability added through bug 635673
    let docShell = getDocShell(this._frame);
    if (docShell && "allowWindowControl" in docShell)
      docShell.allowWindowControl = false;

    // perform all deferred tasks like initSymbiont, show, hide ...
    // TODO: We're publicly exposing a private event here; this
    // 'inited' event should really be made private, somehow.
    this._emit('inited');
  },

  // Catch document unload event in order to rebind load event listener with
  // Symbiont._initFrame if Worker._documentUnload destroyed the worker
  _documentUnload: function(subject, topic, data) {
    if (this._workerDocumentUnload(subject, topic, data)) {
      this._initFrame(this._frame);
      return true;
    }
    return false;
  },

  _onChange: function _onChange(e) {
    this._frameLoadersSwapped = false;
    if ('contentURL' in e && this._frame) {
      // Cleanup the worker before injecting the content script in the new
      // document
      this._workerCleanup();
      this._initFrame(this._frame);
    }
  }
});
exports.Panel = function(options) Panel(options)
exports.Panel.prototype = Panel.prototype;
