/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const HTML_NS = "http://www.w3.org/1999/xhtml";
const EPSILON = 0.001;
const RESIZE_REFRESH_RATE = 50; // ms
const REQUESTS_REFRESH_RATE = 50; // ms
const REQUESTS_HEADERS_SAFE_BOUNDS = 30; // px
const REQUESTS_WATERFALL_SAFE_BOUNDS = 90; // px
const REQUESTS_WATERFALL_HEADER_TICKS_MULTIPLE = 5; // ms
const REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN = 60; // px
const REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5; // ms
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES = 3;
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10; // px
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 10; // byte
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 16; // byte
const DEFAULT_HTTP_VERSION = "HTTP/1.1";
const HEADERS_SIZE_DECIMALS = 3;
const CONTENT_SIZE_DECIMALS = 2;
const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};
const CONTENT_MIME_TYPE_MAPPINGS = {
  "/ecmascript": SourceEditor.MODES.JAVASCRIPT,
  "/javascript": SourceEditor.MODES.JAVASCRIPT,
  "/x-javascript": SourceEditor.MODES.JAVASCRIPT,
  "/html": SourceEditor.MODES.HTML,
  "/xhtml": SourceEditor.MODES.HTML,
  "/xml": SourceEditor.MODES.HTML,
  "/atom": SourceEditor.MODES.HTML,
  "/soap": SourceEditor.MODES.HTML,
  "/rdf": SourceEditor.MODES.HTML,
  "/rss": SourceEditor.MODES.HTML,
  "/css": SourceEditor.MODES.CSS
};
const DEFAULT_EDITOR_CONFIG = {
  mode: SourceEditor.MODES.TEXT,
  readOnly: true,
  showLineNumbers: true
};
const GENERIC_VARIABLES_VIEW_SETTINGS = {
  lazyEmpty: false,
  lazyEmptyDelay: 10, // ms
  searchEnabled: true,
  descriptorTooltip: false,
  editableValueTooltip: "",
  editableNameTooltip: "",
  preventDisableOnChage: true,
  eval: () => {},
  switch: () => {}
};

/**
 * Object defining the network monitor view components.
 */
let NetMonitorView = {
  /**
   * Initializes the network monitor view.
   *
   * @param function aCallback
   *        Called after the view finishes initializing.
   */
  initialize: function(aCallback) {
    dumpn("Initializing the NetMonitorView");

    this._initializePanes();

    this.Toolbar.initialize();
    this.RequestsMenu.initialize();
    this.NetworkDetails.initialize();

    aCallback();
  },

  /**
   * Destroys the network monitor view.
   *
   * @param function aCallback
   *        Called after the view finishes destroying.
   */
  destroy: function(aCallback) {
    dumpn("Destroying the NetMonitorView");

    this.Toolbar.destroy();
    this.RequestsMenu.destroy();
    this.NetworkDetails.destroy();

    this._destroyPanes();

    aCallback();
  },

  /**
   * Initializes the UI for all the displayed panes.
   */
  _initializePanes: function() {
    dumpn("Initializing the NetMonitorView panes");

    this._detailsPane = $("#details-pane");
    this._detailsPaneToggleButton = $("#details-pane-toggle");

    this._collapsePaneString = L10N.getStr("collapseDetailsPane");
    this._expandPaneString = L10N.getStr("expandDetailsPane");

    this._detailsPane.setAttribute("width", Prefs.networkDetailsWidth);
    this._detailsPane.setAttribute("height", Prefs.networkDetailsHeight);
    this.toggleDetailsPane({ visible: false });
  },

  /**
   * Destroys the UI for all the displayed panes.
   */
  _destroyPanes: function() {
    dumpn("Destroying the NetMonitorView panes");

    Prefs.networkDetailsWidth = this._detailsPane.getAttribute("width");
    Prefs.networkDetailsHeight = this._detailsPane.getAttribute("height");

    this._detailsPane = null;
    this._detailsPaneToggleButton = null;
  },

  /**
   * Gets the visibility state of the network details pane.
   * @return boolean
   */
  get detailsPaneHidden()
    this._detailsPane.hasAttribute("pane-collapsed"),

  /**
   * Sets the network details pane hidden or visible.
   *
   * @param object aFlags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   * @param number aTabIndex [optional]
   *        The index of the intended selected tab in the details pane.
   */
  toggleDetailsPane: function(aFlags, aTabIndex) {
    let pane = this._detailsPane;
    let button = this._detailsPaneToggleButton;

    ViewHelpers.togglePane(aFlags, pane);

    if (aFlags.visible) {
      button.removeAttribute("pane-collapsed");
      button.setAttribute("tooltiptext", this._collapsePaneString);
    } else {
      button.setAttribute("pane-collapsed", "");
      button.setAttribute("tooltiptext", this._expandPaneString);
    }

    if (aTabIndex !== undefined) {
      $("#details-pane").selectedIndex = aTabIndex;
    }
  },

  /**
   * Lazily initializes and returns a promise for a SourceEditor instance.
   *
   * @param string aId
   *        The id of the editor placeholder node.
   * @return object
   *         A Promise that is resolved when the editor is available.
   */
  editor: function(aId) {
    dumpn("Getting a NetMonitorView editor: " + aId);

    if (this._editorPromises.has(aId)) {
      return this._editorPromises.get(aId);
    }

    let deferred = Promise.defer();
    this._editorPromises.set(aId, deferred.promise);

    // Initialize the source editor and store the newly created instance
    // in the ether of a resolved promise's value.
    new SourceEditor().init($(aId), DEFAULT_EDITOR_CONFIG, deferred.resolve);

    return deferred.promise;
  },

  _editorPromises: new Map(),
  _collapsePaneString: "",
  _expandPaneString: "",
  _isInitialized: false,
  _isDestroyed: false
};

/**
 * Functions handling the toolbar view: expand/collapse button etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");

  this._onTogglePanesPressed = this._onTogglePanesPressed.bind(this);
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the ToolbarView");

    this._detailsPaneToggleButton = $("#details-pane-toggle");
    this._detailsPaneToggleButton.addEventListener("mousedown", this._onTogglePanesPressed, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the ToolbarView");

    this._detailsPaneToggleButton.removeEventListener("mousedown", this._onTogglePanesPressed, false);
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function() {
    let requestsMenu = NetMonitorView.RequestsMenu;
    let networkDetails = NetMonitorView.NetworkDetails;

    // Make sure there's a selection if the button is pressed, to avoid
    // showing an empty network details pane.
    if (!requestsMenu.selectedItem && requestsMenu.itemCount) {
      requestsMenu.selectedIndex = 0;
    }
    // Proceed with toggling the network details pane normally.
    else {
      networkDetails.toggle(NetMonitorView.detailsPaneHidden);
    }
  },

  _detailsPaneToggleButton: null
};

/**
 * Functions handling the requests menu (containing details about each request,
 * like status, method, file, domain, as well as a waterfall representing
 * timing imformation).
 */
function RequestsMenuView() {
  dumpn("RequestsMenuView was instantiated");

  this._cache = new Map(); // Can't use a WeakMap because keys are strings.
  this._flushRequests = this._flushRequests.bind(this);
  this._onRequestItemRemoved = this._onRequestItemRemoved.bind(this);
  this._onMouseDown = this._onMouseDown.bind(this);
  this._onSelect = this._onSelect.bind(this);
  this._onResize = this._onResize.bind(this);
}

create({ constructor: RequestsMenuView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize: function() {
    dumpn("Initializing the RequestsMenuView");

    this.node = new SideMenuWidget($("#requests-menu-contents"), false);
    this.node.maintainSelectionVisible = false;
    this.node.autoscrollWithAppendedItems = true;

    this.node.addEventListener("mousedown", this._onMouseDown, false);
    this.node.addEventListener("select", this._onSelect, false);
    window.addEventListener("resize", this._onResize, false);
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy: function() {
    dumpn("Destroying the SourcesView");

    this.node.removeEventListener("mousedown", this._onMouseDown, false);
    this.node.removeEventListener("select", this._onSelect, false);
    window.removeEventListener("resize", this._onResize, false);
  },

  /**
   * Resets this container (removes all the networking information).
   */
  reset: function() {
    this.empty();
    this._firstRequestStartedMillis = -1;
    this._lastRequestEndedMillis = -1;
  },

  /**
   * Specifies if this view may be updated lazily.
   */
  lazyUpdate: true,

  /**
   * Adds a network request to this container.
   *
   * @param string aId
   *        An identifier coming from the network monitor controller.
   * @param string aStartedDateTime
   *        A string representation of when the request was started, which
   *        can be parsed by Date (for example "2012-09-17T19:50:03.699Z").
   * @param string aMethod
   *        Specifies the request method (e.g. "GET", "POST", etc.)
   * @param string aUrl
   *        Specifies the request's url.
   */
  addRequest: function(aId, aStartedDateTime, aMethod, aUrl) {
    // Convert the received date/time string to a unix timestamp.
    let unixTime = Date.parse(aStartedDateTime);

    // Create the element node for the network request item.
    let menuView = this._createMenuView(aMethod, aUrl);

    // Remember the first and last event boundaries.
    this._registerFirstRequestStart(unixTime);
    this._registerLastRequestEnd(unixTime);

    // Append a network request item to this container.
    let requestItem = this.push(menuView, {
      attachment: {
        id: aId,
        startedDeltaMillis: unixTime - this._firstRequestStartedMillis,
        startedMillis: unixTime,
        method: aMethod,
        url: aUrl
      },
      finalize: this._onRequestItemRemoved
    });

    $("#details-pane-toggle").disabled = false;
    $(".requests-menu-empty-notice").hidden = true;

    this._cache.set(aId, requestItem);
  },

  /**
   * Sorts all network requests in this container by a specified detail.
   *
   * @param string aType
   *        Either "status", "method", "file", "domain", "type" or "size".
   */
  sortBy: function(aType) {
    let target = $("#requests-menu-" + aType + "-button");
    let headers = document.querySelectorAll(".requests-menu-header-button");

    for (let header of headers) {
      if (header != target) {
        header.removeAttribute("sorted");
        header.removeAttribute("tooltiptext");
      }
    }

    let direction = "";
    if (target) {
      if (!target.hasAttribute("sorted")) {
        target.setAttribute("sorted", direction = "ascending");
        target.setAttribute("tooltiptext", L10N.getStr("networkMenu.sortedAsc"));
      } else if (target.getAttribute("sorted") == "ascending") {
        target.setAttribute("sorted", direction = "descending");
        target.setAttribute("tooltiptext", L10N.getStr("networkMenu.sortedDesc"));
      } else {
        target.removeAttribute("sorted");
        target.removeAttribute("tooltiptext");
      }
    }

    // Sort by timing.
    if (!target || !direction) {
      this.sortContents(this._byTiming);
    }
    // Sort by whatever was requested.
    else switch (aType) {
      case "status":
        if (direction == "ascending") {
          this.sortContents(this._byStatus);
        } else {
          this.sortContents((a, b) => !this._byStatus(a, b));
        }
        break;
      case "method":
        if (direction == "ascending") {
          this.sortContents(this._byMethod);
        } else {
          this.sortContents((a, b) => !this._byMethod(a, b));
        }
        break;
      case "file":
        if (direction == "ascending") {
          this.sortContents(this._byFile);
        } else {
          this.sortContents((a, b) => !this._byFile(a, b));
        }
        break;
      case "domain":
        if (direction == "ascending") {
          this.sortContents(this._byDomain);
        } else {
          this.sortContents((a, b) => !this._byDomain(a, b));
        }
        break;
      case "type":
        if (direction == "ascending") {
          this.sortContents(this._byType);
        } else {
          this.sortContents((a, b) => !this._byType(a, b));
        }
        break;
      case "size":
        if (direction == "ascending") {
          this.sortContents(this._bySize);
        } else {
          this.sortContents((a, b) => !this._bySize(a, b));
        }
        break;
    }
  },

  /**
   * Predicates used when sorting items.
   *
   * @param MenuItem aFirst
   *        The first menu item used in the comparison.
   * @param MenuItem aSecond
   *        The second menu item used in the comparison.
   * @return number
   *         -1 to sort aFirst to a lower index than aSecond
   *          0 to leave aFirst and aSecond unchanged with respect to each other
   *          1 to sort aSecond to a lower index than aFirst
   */
  _byTiming: (aFirst, aSecond) =>
    aFirst.attachment.startedMillis > aSecond.attachment.startedMillis,

  _byStatus: (aFirst, aSecond) =>
    aFirst.attachment.status > aSecond.attachment.status,

  _byMethod: (aFirst, aSecond) =>
    aFirst.attachment.method > aSecond.attachment.method,

  _byFile: (aFirst, aSecond) =>
    !aFirst.target || !aSecond.target ? -1 :
      $(".requests-menu-file", aFirst.target).getAttribute("value").toLowerCase() >
      $(".requests-menu-file", aSecond.target).getAttribute("value").toLowerCase(),

  _byDomain: (aFirst, aSecond) =>
    !aFirst.target || !aSecond.target ? -1 :
      $(".requests-menu-domain", aFirst.target).getAttribute("value").toLowerCase() >
      $(".requests-menu-domain", aSecond.target).getAttribute("value").toLowerCase(),

  _byType: (aFirst, aSecond) =>
    !aFirst.target || !aSecond.target ? -1 :
      $(".requests-menu-type", aFirst.target).getAttribute("value").toLowerCase() >
      $(".requests-menu-type", aSecond.target).getAttribute("value").toLowerCase(),

  _bySize: (aFirst, aSecond) =>
    aFirst.attachment.contentSize > aSecond.attachment.contentSize,

  /**
   * Schedules adding additional information to a network request.
   *
   * @param string aId
   *        An identifier coming from the network monitor controller.
   * @param object aData
   *        An object containing several { key: value } tuples of network info.
   *        Supported keys are "httpVersion", "status", "statusText" etc.
   */
  updateRequest: function(aId, aData) {
    // Prevent interference from zombie updates received after target closed.
    if (NetMonitorView._isDestroyed) {
      return;
    }
    this._updateQueue.push([aId, aData]);

    // Lazy updating is disabled in some tests.
    if (!this.lazyUpdate) {
      return void this._flushRequests();
    }
    // Allow requests to settle down first.
    drain("update-requests", REQUESTS_REFRESH_RATE, () => this._flushRequests());
  },

  /**
   * Starts adding all queued additional information about network requests.
   */
  _flushRequests: function() {
    // For each queued additional information packet, get the corresponding
    // request item in the view and update it based on the specified data.
    for (let [id, data] of this._updateQueue) {
      let requestItem = this._cache.get(id);
      if (!requestItem) {
        // Packet corresponds to a dead request item, target navigated.
        continue;
      }

      // Each information packet may contain several { key: value } tuples of
      // network info, so update the view based on each one.
      for (let key in data) {
        let value = data[key];
        if (value === undefined) {
          // The information in the packet is empty, it can be safely ignored.
          continue;
        }

        switch (key) {
          case "requestHeaders":
            requestItem.attachment.requestHeaders = value;
            break;
          case "requestCookies":
            requestItem.attachment.requestCookies = value;
            break;
          case "requestPostData":
            requestItem.attachment.requestPostData = value;
            break;
          case "responseHeaders":
            requestItem.attachment.responseHeaders = value;
            break;
          case "responseCookies":
            requestItem.attachment.responseCookies = value;
            break;
          case "httpVersion":
            requestItem.attachment.httpVersion = value;
            break;
          case "status":
            requestItem.attachment.status = value;
            this._updateMenuView(requestItem, key, value);
            break;
          case "statusText":
            requestItem.attachment.statusText = value;
            this._updateMenuView(requestItem, key,
              requestItem.attachment.status + " " +
              requestItem.attachment.statusText);
            break;
          case "headersSize":
            requestItem.attachment.headersSize = value;
            break;
          case "contentSize":
            requestItem.attachment.contentSize = value;
            this._updateMenuView(requestItem, key, value);
            break;
          case "mimeType":
            requestItem.attachment.mimeType = value;
            this._updateMenuView(requestItem, key, value);
            break;
          case "responseContent":
            requestItem.attachment.responseContent = value;
            break;
          case "totalTime":
            requestItem.attachment.totalTime = value;
            requestItem.attachment.endedMillis = requestItem.attachment.startedMillis + value;
            this._updateMenuView(requestItem, key, value);
            this._registerLastRequestEnd(requestItem.attachment.endedMillis);
            break;
          case "eventTimings":
            requestItem.attachment.eventTimings = value;
            this._createWaterfallView(requestItem, value.timings);
            break;
        }
      }
      // This update may have additional information about a request which
      // isn't shown yet in the network details pane.
      let selectedItem = this.selectedItem;
      if (selectedItem && selectedItem.attachment.id == id) {
        NetMonitorView.NetworkDetails.populate(selectedItem.attachment);
      }
    }

    // We're done flushing all the requests, clear the update queue.
    this._updateQueue = [];

    // Make sure all the requests are sorted.
    this.sortContents();
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aMethod
   *        Specifies the request method (e.g. "GET", "POST", etc.)
   * @param string aUrl
   *        Specifies the request's url.
   * @return nsIDOMNode
   *         The network request view.
   */
  _createMenuView: function(aMethod, aUrl) {
    let uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
    let name = NetworkHelper.convertToUnicode(unescape(uri.fileName)) || "/";
    let query = NetworkHelper.convertToUnicode(unescape(uri.query));
    let hostPort = NetworkHelper.convertToUnicode(unescape(uri.hostPort));

    let template = $("#requests-menu-item-template");
    let fragment = document.createDocumentFragment();

    $(".requests-menu-method", template).setAttribute("value", aMethod);

    let file = $(".requests-menu-file", template);
    file.setAttribute("value", name + (query ? "?" + query : ""));
    file.setAttribute("tooltiptext", name + (query ? "?" + query : ""));

    let domain = $(".requests-menu-domain", template);
    domain.setAttribute("value", hostPort);
    domain.setAttribute("tooltiptext", hostPort);

    // Flatten the DOM by removing one redundant box (the template container).
    for (let node of template.childNodes) {
      fragment.appendChild(node.cloneNode(true));
    }

    return fragment;
  },

  /**
   * Updates the information displayed in a network request item view.
   *
   * @param MenuItem aItem
   *        The network request item in this container.
   * @param string aKey
   *        The type of information that is to be updated.
   * @param any aValue
   *        The new value to be shown.
   */
  _updateMenuView: function(aItem, aKey, aValue) {
    switch (aKey) {
      case "status": {
        let node = $(".requests-menu-status", aItem.target);
        node.setAttribute("code", aValue);
        break;
      }
      case "statusText": {
        let node = $(".requests-menu-status-and-method", aItem.target);
        node.setAttribute("tooltiptext", aValue);
        break;
      }
      case "contentSize": {
        let kb = aValue / 1024;
        let size = L10N.numberWithDecimals(kb, CONTENT_SIZE_DECIMALS);
        let node = $(".requests-menu-size", aItem.target);
        let text = L10N.getFormatStr("networkMenu.sizeKB", size);
        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", text);
        break;
      }
      case "mimeType": {
        let type = aValue.split(";")[0].split("/")[1] || "?";
        let node = $(".requests-menu-type", aItem.target);
        let text = CONTENT_MIME_TYPE_ABBREVIATIONS[type] || type;
        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", aValue);
        break;
      }
      case "totalTime": {
        let node = $(".requests-menu-timings-total", aItem.target);
        let text = L10N.getFormatStr("networkMenu.totalMS", aValue); // integer
        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", text);
        break;
      }
    }
  },

  /**
   * Creates a waterfall representing timing information in a network request item view.
   *
   * @param MenuItem aItem
   *        The network request item in this container.
   * @param object aTimings
   *        An object containing timing information.
   */
  _createWaterfallView: function(aItem, aTimings) {
    let { target, attachment } = aItem;
    let sections = ["dns", "connect", "send", "wait", "receive"];
    // Skipping "blocked" because it doesn't work yet.

    let timingsNode = $(".requests-menu-timings", target);
    let startCapNode = $(".requests-menu-timings-cap.start", timingsNode);
    let endCapNode = $(".requests-menu-timings-cap.end", timingsNode);
    let firstBox;

    // Add a set of boxes representing timing information.
    for (let key of sections) {
      let width = aTimings[key];

      // Don't render anything if it surely won't be visible.
      // One millisecond == one unscaled pixel.
      if (width > 0) {
        let timingBox = document.createElement("hbox");
        timingBox.className = "requests-menu-timings-box " + key;
        timingBox.setAttribute("width", width);
        timingsNode.insertBefore(timingBox, endCapNode);

        // Make the start cap inherit the aspect of the first timing box.
        if (!firstBox) {
          firstBox = timingBox;
          startCapNode.classList.add(key);
        }
        // Same goes for the end cap, inherit the aspect of the last timing box.
        endCapNode.classList.add(key);
      }
    }

    // Since at least one timing box should've been rendered, unhide the
    // start and end timing cap nodes.
    startCapNode.hidden = false;
    endCapNode.hidden = false;

    // Rescale all the waterfalls so that everything is visible at once.
    this._flushWaterfallViews();
  },

  /**
   * Rescales and redraws all the waterfall views in this container.
   *
   * @param boolean aReset
   *        True if this container's width was changed.
   */
  _flushWaterfallViews: function(aReset) {
    // To avoid expensive operations like getBoundingClientRect() and
    // rebuilding the waterfall background each time a new request comes in,
    // stuff is cached. However, in certain scenarios like when the window
    // is resized, this needs to be invalidated.
    if (aReset) {
      this._cachedWaterfallWidth = 0;
      this._hideOverflowingColumns();
    }

    // Determine the scaling to be applied to all the waterfalls so that
    // everything is visible at once. One millisecond == one unscaled pixel.
    let availableWidth = this._waterfallWidth - REQUESTS_WATERFALL_SAFE_BOUNDS;
    let longestWidth = this._lastRequestEndedMillis - this._firstRequestStartedMillis;
    let scale = Math.min(Math.max(availableWidth / longestWidth, EPSILON), 1);

    // Redraw and set the canvas background for each waterfall view.
    this._showWaterfallDivisionLabels(scale);
    this._drawWaterfallBackground(scale);
    this._flushWaterfallBackgrounds();

    // Apply CSS transforms to each waterfall in this container totalTime
    // accurately translate and resize as needed.
    for (let [, { target, attachment }] of this._cache) {
      let timingsNode = $(".requests-menu-timings", target);
      let startCapNode = $(".requests-menu-timings-cap.start", target);
      let endCapNode = $(".requests-menu-timings-cap.end", target);
      let totalNode = $(".requests-menu-timings-total", target);

      // Render the timing information at a specific horizontal translation
      // based on the delta to the first monitored event network.
      let translateX = "translateX(" + attachment.startedDeltaMillis + "px)";

      // Based on the total time passed until the last request, rescale
      // all the waterfalls to a reasonable size.
      let scaleX = "scaleX(" + scale + ")";

      // Certain nodes should not be scaled, even if they're children of
      // another scaled node. In this case, apply a reversed transformation.
      let revScaleX = "scaleX(" + (1 / scale) + ")";

      timingsNode.style.transform = scaleX + " " + translateX;
      startCapNode.style.transform = revScaleX + " translateX(0.5px)";
      endCapNode.style.transform = revScaleX + " translateX(-0.5px)";
      totalNode.style.transform = revScaleX;
    }
  },

  /**
   * Creates the labels displayed on the waterfall header in this container.
   *
   * @param number aScale
   *        The current waterfall scale.
   */
  _showWaterfallDivisionLabels: function(aScale) {
    let container = $("#requests-menu-waterfall-header-box");
    let availableWidth = this._waterfallWidth - REQUESTS_WATERFALL_SAFE_BOUNDS;

    // Nuke all existing labels.
    while (container.hasChildNodes()) {
      container.firstChild.remove();
    }

    // Build new millisecond tick labels...
    let timingStep = REQUESTS_WATERFALL_HEADER_TICKS_MULTIPLE;
    let optimalTickIntervalFound = false;

    while (!optimalTickIntervalFound) {
      // Ignore any divisions that would end up being too close to each other.
      let scaledStep = aScale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;

      // Insert one label for each division on the current scale.
      let fragment = document.createDocumentFragment();

      for (let x = 0; x < availableWidth; x += scaledStep) {
        let divisionMS = (x / aScale).toFixed(0);
        let translateX = "translateX(" + (x | 0) + "px)";

        let node = document.createElement("label");
        let text = L10N.getFormatStr("networkMenu.divisionMS", divisionMS);
        node.className = "plain requests-menu-timings-division";
        node.style.transform = translateX;

        node.setAttribute("value", text);
        fragment.appendChild(node);
      }
      container.appendChild(fragment);
    }
  },

  /**
   * Creates the background displayed on each waterfall view in this container.
   *
   * @param number aScale
   *        The current waterfall scale.
   */
  _drawWaterfallBackground: function(aScale) {
    if (!this._canvas || !this._ctx) {
      this._canvas = document.createElementNS(HTML_NS, "canvas");
      this._ctx = this._canvas.getContext("2d");
    }
    let canvas = this._canvas;
    let ctx = this._ctx;

    // Nuke the context.
    let canvasWidth = canvas.width = this._waterfallWidth;
    let canvasHeight = canvas.height = 1; // Awww yeah, 1px, repeats on Y axis.

    // Start over.
    let imageData = ctx.createImageData(canvasWidth, canvasHeight);
    let pixelArray = imageData.data;

    let buf = new ArrayBuffer(pixelArray.length);
    let buf8 = new Uint8ClampedArray(buf);
    let data32 = new Uint32Array(buf);

    // Build new millisecond tick lines...
    let timingStep = REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE;
    let alphaComponent = REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN;
    let optimalTickIntervalFound = false;

    while (!optimalTickIntervalFound) {
      // Ignore any divisions that would end up being too close to each other.
      let scaledStep = aScale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;

      // Insert one pixel for each division on each scale.
      for (let i = 1; i <= REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES; i++) {
        let increment = scaledStep * Math.pow(2, i);
        for (let x = 0; x < canvasWidth; x += increment) {
          data32[x | 0] = (alphaComponent << 24) | (255 << 16) | (255 <<  8) | 255;
        }
        alphaComponent += REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD;
      }
    }

    // Flush the image data and cache the waterfall background.
    pixelArray.set(buf8);
    ctx.putImageData(imageData, 0, 0);
    this._cachedWaterfallBackground = "url(" + canvas.toDataURL() + ")";
  },

  /**
   * Reapplies the current waterfall background on all request items.
   */
  _flushWaterfallBackgrounds: function() {
    for (let [, { target }] of this._cache) {
      let waterfallNode = $(".requests-menu-waterfall", target);
      waterfallNode.style.backgroundImage = this._cachedWaterfallBackground;
    }
  },

  /**
   * Hides the overflowing columns in the requests table.
   */
  _hideOverflowingColumns: function() {
    let table = $("#network-table");
    let toolbar = $("#requests-menu-toolbar");
    let columns = [
      ["#requests-menu-waterfall-header-box", "waterfall-overflows"],
      ["#requests-menu-size-header-box", "size-overflows"],
      ["#requests-menu-type-header-box", "type-overflows"],
      ["#requests-menu-domain-header-box", "domain-overflows"]
    ];

    // Flush headers.
    columns.forEach(([, attribute]) => table.removeAttribute(attribute));
    let availableWidth = toolbar.getBoundingClientRect().width;

    // Hide the columns.
    columns.forEach(([id, attribute]) => {
      let bounds = $(id).getBoundingClientRect();
      if (bounds.right > availableWidth - REQUESTS_HEADERS_SAFE_BOUNDS) {
        table.setAttribute(attribute, "");
      }
    });
  },

  /**
   * Function called each time a network request item is removed.
   *
   * @param MenuItem aItem
   *        The corresponding menu item.
   */
  _onRequestItemRemoved: function(aItem) {
    dumpn("Finalizing network request item: " + aItem);
    this._cache.delete(aItem.attachment.id);
  },

  /**
   * The mouse down listener for this container.
   */
  _onMouseDown: function(e) {
    let item = this.getItemForElement(e.target);
    if (item) {
      // The container is not empty and we clicked on an actual item.
      this.selectedItem = item;
    }
  },

  /**
   * The selection listener for this container.
   */
  _onSelect: function(e) {
    NetMonitorView.NetworkDetails.populate(this.selectedItem.attachment);
    NetMonitorView.NetworkDetails.toggle(true);
  },

  /**
   * The resize listener for this container's window.
   */
  _onResize: function(e) {
    // Allow requests to settle down first.
    drain("resize-events", RESIZE_REFRESH_RATE, () => this._flushWaterfallViews(true));
  },

  /**
   * Checks if the specified unix time is the first one to be known of,
   * and saves it if so.
   *
   * @param number aUnixTime
   *        The milliseconds to check and save.
   */
  _registerFirstRequestStart: function(aUnixTime) {
    if (this._firstRequestStartedMillis == -1) {
      this._firstRequestStartedMillis = aUnixTime;
    }
  },

  /**
   * Checks if the specified unix time is the last one to be known of,
   * and saves it if so.
   *
   * @param number aUnixTime
   *        The milliseconds to check and save.
   */
  _registerLastRequestEnd: function(aUnixTime) {
    if (this._lastRequestEndedMillis < aUnixTime) {
      this._lastRequestEndedMillis = aUnixTime;
    }
  },

  /**
   * Gets the available waterfall width in this container.
   * @return number
   */
  get _waterfallWidth() {
    if (this._cachedWaterfallWidth == 0) {
      let container = $("#requests-menu-toolbar");
      let waterfall = $("#requests-menu-waterfall-header-box");
      let containerBounds = container.getBoundingClientRect();
      let waterfallBounds = waterfall.getBoundingClientRect();
      this._cachedWaterfallWidth = containerBounds.width - waterfallBounds.left;
    }
    return this._cachedWaterfallWidth;
  },

  _cache: null,
  _canvas: null,
  _ctx: null,
  _cachedWaterfallWidth: 0,
  _cachedWaterfallBackground: null,
  _firstRequestStartedMillis: -1,
  _lastRequestEndedMillis: -1,
  _updateQueue: [],
  _updateTimeout: null,
  _resizeTimeout: null
});

/**
 * Functions handling the requests details view.
 */
function NetworkDetailsView() {
  dumpn("NetworkDetailsView was instantiated");
};

create({ constructor: NetworkDetailsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize: function() {
    dumpn("Initializing the RequestsMenuView");

    this.node = $("#details-pane");

    this._headers = new VariablesView($("#all-headers"),
      Object.create(GENERIC_VARIABLES_VIEW_SETTINGS, {
        emptyText: { value: L10N.getStr("headersEmptyText"), enumerable: true },
        searchPlaceholder: { value: L10N.getStr("headersFilterText"), enumerable: true }
      }));
    this._cookies = new VariablesView($("#all-cookies"),
      Object.create(GENERIC_VARIABLES_VIEW_SETTINGS, {
        emptyText: { value: L10N.getStr("cookiesEmptyText"), enumerable: true },
        searchPlaceholder: { value: L10N.getStr("cookiesFilterText"), enumerable: true }
      }));
    this._params = new VariablesView($("#request-params"),
      Object.create(GENERIC_VARIABLES_VIEW_SETTINGS, {
        emptyText: { value: L10N.getStr("paramsEmptyText"), enumerable: true },
        searchPlaceholder: { value: L10N.getStr("paramsFilterText"), enumerable: true }
      }));
    this._json = new VariablesView($("#response-content-json"),
      Object.create(GENERIC_VARIABLES_VIEW_SETTINGS, {
        searchPlaceholder: { value: L10N.getStr("jsonFilterText"), enumerable: true }
      }));

    this._paramsQueryString = L10N.getStr("paramsQueryString");
    this._paramsFormData = L10N.getStr("paramsFormData");
    this._paramsPostPayload = L10N.getStr("paramsPostPayload");
    this._requestHeaders = L10N.getStr("requestHeaders");
    this._responseHeaders = L10N.getStr("responseHeaders");
    this._requestCookies = L10N.getStr("requestCookies");
    this._responseCookies = L10N.getStr("responseCookies");
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy: function() {
    dumpn("Destroying the SourcesView");
  },

  /**
   * Sets this view hidden or visible. It's visible by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggle: function(aVisibleFlag) {
    NetMonitorView.toggleDetailsPane({ visible: aVisibleFlag });
    NetMonitorView.RequestsMenu._flushWaterfallViews(true);
  },

  /**
   * Populates this view with the specified data.
   *
   * @param object aData
   *        The data source (this should be the attachment of a request item).
   */
  populate: function(aData) {
    $("#request-params-box").setAttribute("flex", "1");
    $("#request-params-box").hidden = false;
    $("#request-post-data-textarea-box").hidden = true;
    $("#response-content-json-box").hidden = true;
    $("#response-content-textarea-box").hidden = true;
    $("#response-content-image-box").hidden = true;

    this._headers.empty();
    this._cookies.empty();
    this._params.empty();
    this._json.empty();

    if (aData) {
      this._setSummary(aData);
      this._setResponseHeaders(aData.responseHeaders);
      this._setRequestHeaders(aData.requestHeaders);
      this._setResponseCookies(aData.responseCookies);
      this._setRequestCookies(aData.requestCookies);
      this._setRequestGetParams(aData.url);
      this._setRequestPostParams(aData.requestHeaders, aData.requestPostData);
      this._setResponseBody(aData.url, aData.responseContent);
      this._setTimingsInformation(aData.eventTimings);
    }
  },

  /**
   * Sets the network request summary shown in this view.
   *
   * @param object aData
   *        The data source (this should be the attachment of a request item).
   */
  _setSummary: function(aData) {
    if (aData.url) {
      let unicodeUrl = NetworkHelper.convertToUnicode(unescape(aData.url));
      $("#headers-summary-url-value").setAttribute("value", unicodeUrl);
      $("#headers-summary-url-value").setAttribute("tooltiptext", unicodeUrl);
      $("#headers-summary-url").removeAttribute("hidden");
    } else {
      $("#headers-summary-url").setAttribute("hidden", "true");
    }

    if (aData.method) {
      $("#headers-summary-method-value").setAttribute("value", aData.method);
      $("#headers-summary-method").removeAttribute("hidden");
    } else {
      $("#headers-summary-method").setAttribute("hidden", "true");
    }

    if (aData.status) {
      $("#headers-summary-status-circle").setAttribute("code", aData.status);
      $("#headers-summary-status-value").setAttribute("value", aData.status + " " + aData.statusText);
      $("#headers-summary-status").removeAttribute("hidden");
    } else {
      $("#headers-summary-status").setAttribute("hidden", "true");
    }

    if (aData.httpVersion && aData.httpVersion != DEFAULT_HTTP_VERSION) {
      $("#headers-summary-version-value").setAttribute("value", aData.httpVersion);
      $("#headers-summary-version").removeAttribute("hidden");
    } else {
      $("#headers-summary-version").setAttribute("hidden", "true");
    }
  },

  /**
   * Sets the network request headers shown in this view.
   *
   * @param object aResponse
   *        The message received from the server.
   */
  _setRequestHeaders: function(aResponse) {
    if (aResponse && aResponse.headers.length) {
      this._addHeaders(this._requestHeaders, aResponse);
    }
  },

  /**
   * Sets the network response headers shown in this view.
   *
   * @param object aResponse
   *        The message received from the server.
   */
  _setResponseHeaders: function(aResponse) {
    if (aResponse && aResponse.headers.length) {
      aResponse.headers.sort((a, b) => a.name > b.name);
      this._addHeaders(this._responseHeaders, aResponse);
    }
  },

  /**
   * Populates the headers container in this view with the specified data.
   *
   * @param string aName
   *        The type of headers to populate (request or response).
   * @param object aResponse
   *        The message received from the server.
   */
  _addHeaders: function(aName, aResponse) {
    let kb = aResponse.headersSize / 1024;
    let size = L10N.numberWithDecimals(kb, HEADERS_SIZE_DECIMALS);
    let text = L10N.getFormatStr("networkMenu.sizeKB", size);
    let headersScope = this._headers.addScope(aName + " (" + text + ")");
    headersScope.expanded = true;

    for (let header of aResponse.headers) {
      let headerVar = headersScope.addVar(header.name, { null: true }, true);
      gNetwork.getString(header.value).then((aString) => headerVar.setGrip(aString));
    }
  },

  /**
   * Sets the network request cookies shown in this view.
   *
   * @param object aResponse
   *        The message received from the server.
   */
  _setRequestCookies: function(aResponse) {
    if (aResponse && aResponse.cookies.length) {
      aResponse.cookies.sort((a, b) => a.name > b.name);
      this._addCookies(this._requestCookies, aResponse);
    }
  },

  /**
   * Sets the network response cookies shown in this view.
   *
   * @param object aResponse
   *        The message received from the server.
   */
  _setResponseCookies: function(aResponse) {
    if (aResponse && aResponse.cookies.length) {
      this._addCookies(this._responseCookies, aResponse);
    }
  },

  /**
   * Populates the cookies container in this view with the specified data.
   *
   * @param string aName
   *        The type of cookies to populate (request or response).
   * @param object aResponse
   *        The message received from the server.
   */
  _addCookies: function(aName, aResponse) {
    let cookiesScope = this._cookies.addScope(aName);
    cookiesScope.expanded = true;

    for (let cookie of aResponse.cookies) {
      let cookieVar = cookiesScope.addVar(cookie.name, { null: true }, true);
      gNetwork.getString(cookie.value).then((aString) => cookieVar.setGrip(aString));

      // By default the cookie name and value are shown. If this is the only
      // information available, then nothing else is to be displayed.
      let cookieProps = Object.keys(cookie);
      if (cookieProps.length == 2) {
        continue;
      }

      // Display any other information other than the cookie name and value
      // which may be available.
      let rawObject = Object.create(null);
      let otherProps = cookieProps.filter((e) => e != "name" && e != "value");
      for (let prop of otherProps) {
        rawObject[prop] = cookie[prop];
      }
      cookieVar.populate(rawObject);
      cookieVar.twisty = true;
      cookieVar.expanded = true;
    }
  },

  /**
   * Sets the network request get params shown in this view.
   *
   * @param string aUrl
   *        The request's url.
   */
  _setRequestGetParams: function(aUrl) {
    let uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
    let query = uri.query;
    if (query) {
      this._addParams(this._paramsQueryString, query);
    }
  },

  /**
   * Sets the network request post params shown in this view.
   *
   * @param object aHeadersResponse
   *        The "requestHeaders" message received from the server.
   * @param object aPostResponse
   *        The "requestPostData" message received from the server.
   */
  _setRequestPostParams: function(aHeadersResponse, aPostResponse) {
    if (!aHeadersResponse || !aPostResponse) {
      return;
    }
    let contentType = aHeadersResponse.headers.filter(({ name }) => name == "Content-Type")[0];
    let text = aPostResponse.postData.text;

    gNetwork.getString(text).then((aString) => {
      // Handle query strings (poor man's forms, e.g. "?foo=bar&baz=42").
      if (contentType.value.contains("x-www-form-urlencoded")) {
        this._addParams(this._paramsFormData, aString);
      }
      // Handle actual forms ("multipart/form-data" content type).
      else {
        // This is really awkward, but hey, it works. Let's show an empty
        // scope in the params view and place the source editor containing
        // the raw post data directly underneath.
        $("#request-params-box").removeAttribute("flex");
        let paramsScope = this._params.addScope(this._paramsPostPayload);
        paramsScope.expanded = true;
        paramsScope.locked = true;

        $("#request-post-data-textarea-box").hidden = false;
        NetMonitorView.editor("#request-post-data-textarea").then((aEditor) => {
          aEditor.setText(aString);
        });
      }
      window.emit("NetMonitor:ResponsePostParamsAvailable");
    });
  },

  /**
   * Populates the params container in this view with the specified data.
   *
   * @param string aName
   *        The type of params to populate (get or post).
   * @param string aParams
   *        A query string of params (e.g. "?foo=bar&baz=42").
   */
  _addParams: function(aName, aParams) {
    // Turn the params string into an array containing { name: value } tuples.
    let paramsArray = aParams.replace(/^[?&]/, "").split("&").map((e) =>
      let (param = e.split("=")) {
        name: NetworkHelper.convertToUnicode(unescape(param[0])),
        value: NetworkHelper.convertToUnicode(unescape(param[1]))
      });

    let paramsScope = this._params.addScope(aName);
    paramsScope.expanded = true;

    for (let param of paramsArray) {
      let headerVar = paramsScope.addVar(param.name, { null: true }, true);
      headerVar.setGrip(param.value);
    }
  },

  /**
   * Sets the network response body shown in this view.
   *
   * @param string aUrl
   *        The request's url.
   * @param object aResponse
   *        The message received from the server.
   */
  _setResponseBody: function(aUrl, aResponse) {
    if (!aResponse) {
      return;
    }
    let uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
    let { mimeType, text, encoding } = aResponse.content;

    gNetwork.getString(text).then((aString) => {
      // Handle json.
      if (mimeType.contains("/json")) {
        $("#response-content-json-box").hidden = false;
        let jsonpRegex = /^[a-zA-Z0-9_$]+\(|\)$/g; // JSONP with callback.
        let sanitizedJSON = aString.replace(jsonpRegex, "");
        let callbackPadding = aString.match(jsonpRegex);

        let jsonScopeName = callbackPadding
          ? L10N.getFormatStr("jsonpScopeName", callbackPadding[0].slice(0, -1))
          : L10N.getStr("jsonScopeName");

        let jsonScope = this._json.addScope(jsonScopeName);
        jsonScope.addVar().populate(JSON.parse(sanitizedJSON), { expanded: true });
        jsonScope.expanded = true;
      }
      // Handle images.
      else if (mimeType.contains("image/")) {
        $("#response-content-image-box").setAttribute("align", "center");
        $("#response-content-image-box").setAttribute("pack", "center");
        $("#response-content-image-box").hidden = false;
        $("#response-content-image").src =
          "data:" + mimeType + ";" + encoding + "," + aString;

        // Immediately display additional information about the image:
        // file name, mime type and encoding.
        $("#response-content-image-name-value").setAttribute("value", uri.fileName);
        $("#response-content-image-mime-value").setAttribute("value", mimeType);
        $("#response-content-image-encoding-value").setAttribute("value", encoding);

        // Wait for the image to load in order to display the width and height.
        $("#response-content-image").onload = (e) => {
          // XUL images are majestic so they don't bother storing their dimensions
          // in width and height attributes like the rest of the folk. Hack around
          // this by getting the bounding client rect and subtracting the margins.
          let { width, height } = e.target.getBoundingClientRect();
          let dimensions = (width - 2) + " x " + (height - 2);
          $("#response-content-image-dimensions-value").setAttribute("value", dimensions);
        };
      }
      // Handle anything else.
      else {
        $("#response-content-textarea-box").hidden = false;
        NetMonitorView.editor("#response-content-textarea").then((aEditor) => {
          aEditor.setMode(SourceEditor.MODES.TEXT);
          aEditor.setText(NetworkHelper.convertToUnicode(aString, "UTF-8"));

          // Maybe set a more appropriate mode in the Source Editor if possible.
          for (let key in CONTENT_MIME_TYPE_MAPPINGS) {
            if (mimeType.contains(key)) {
              aEditor.setMode(CONTENT_MIME_TYPE_MAPPINGS[key]);
              break;
            }
          }
        });
      }
      window.emit("NetMonitor:ResponseBodyAvailable");
    });
  },

  /**
   * Sets the timings information shown in this view.
   *
   * @param object aResponse
   *        The message received from the server.
   */
  _setTimingsInformation: function(aResponse) {
    if (!aResponse) {
      return;
    }
    let { blocked, dns, connect, send, wait, receive } = aResponse.timings;

    let tabboxWidth = $("#details-pane").getAttribute("width");
    let availableWidth = tabboxWidth / 2; // Other nodes also take some space.
    let scale = Math.max(availableWidth / aResponse.totalTime, 0);

    $("#timings-summary-blocked .requests-menu-timings-box")
      .setAttribute("width", blocked * scale);
    $("#timings-summary-blocked .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", blocked));

    $("#timings-summary-dns .requests-menu-timings-box")
      .setAttribute("width", dns * scale);
    $("#timings-summary-dns .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", dns));

    $("#timings-summary-connect .requests-menu-timings-box")
      .setAttribute("width", connect * scale);
    $("#timings-summary-connect .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", connect));

    $("#timings-summary-send .requests-menu-timings-box")
      .setAttribute("width", send * scale);
    $("#timings-summary-send .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", send));

    $("#timings-summary-wait .requests-menu-timings-box")
      .setAttribute("width", wait * scale);
    $("#timings-summary-wait .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", wait));

    $("#timings-summary-receive .requests-menu-timings-box")
      .setAttribute("width", receive * scale);
    $("#timings-summary-receive .requests-menu-timings-total")
      .setAttribute("value", L10N.getFormatStr("networkMenu.totalMS", receive));

    $("#timings-summary-dns .requests-menu-timings-box")
      .style.transform = "translateX(" + (scale * blocked) + "px)";
    $("#timings-summary-connect .requests-menu-timings-box")
      .style.transform = "translateX(" + (scale * (blocked + dns)) + "px)";
    $("#timings-summary-send .requests-menu-timings-box")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect)) + "px)";
    $("#timings-summary-wait .requests-menu-timings-box")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect + send)) + "px)";
    $("#timings-summary-receive .requests-menu-timings-box")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect + send + wait)) + "px)";

    $("#timings-summary-dns .requests-menu-timings-total")
      .style.transform = "translateX(" + (scale * blocked) + "px)";
    $("#timings-summary-connect .requests-menu-timings-total")
      .style.transform = "translateX(" + (scale * (blocked + dns)) + "px)";
    $("#timings-summary-send .requests-menu-timings-total")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect)) + "px)";
    $("#timings-summary-wait .requests-menu-timings-total")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect + send)) + "px)";
    $("#timings-summary-receive .requests-menu-timings-total")
      .style.transform = "translateX(" + (scale * (blocked + dns + connect + send + wait)) + "px)";
  },

  _headers: null,
  _cookies: null,
  _params: null,
  _json: null,
  _paramsQueryString: "",
  _paramsFormData: "",
  _paramsPostPayload: "",
  _requestHeaders: "",
  _responseHeaders: "",
  _requestCookies: "",
  _responseCookies: ""
});

/**
 * DOM query helper.
 */
function $(aSelector, aTarget = document) aTarget.querySelector(aSelector);

/**
 * Helper for draining a rapid succession of events and invoking a callback
 * once everything settles down.
 */
function drain(aId, aWait, aCallback) {
  window.clearTimeout(drain.store.get(aId));
  drain.store.set(aId, window.setTimeout(aCallback, aWait));
}

drain.store = new Map();

/**
 * Preliminary setup for the NetMonitorView object.
 */
NetMonitorView.Toolbar = new ToolbarView();
NetMonitorView.RequestsMenu = new RequestsMenuView();
NetMonitorView.NetworkDetails = new NetworkDetailsView();
