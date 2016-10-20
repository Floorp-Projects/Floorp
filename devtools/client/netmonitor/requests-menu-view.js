/* globals document, window, dumpn, $, gNetwork, EVENTS, Prefs,
           NetMonitorController, NetMonitorView */
"use strict";
/* eslint-disable mozilla/reject-some-requires */
const { Cu } = require("chrome");
const Services = require("Services");
const {Task} = require("devtools/shared/task");
const {DeferredTask} = Cu.import("resource://gre/modules/DeferredTask.jsm", {});
/* eslint-disable mozilla/reject-some-requires */
const {SideMenuWidget} = require("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");
const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
const {setImageTooltip, getImageDimensions} =
  require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper");
const {Heritage, WidgetMethods, setNamedTimeout} =
  require("devtools/client/shared/widgets/view-helpers");
const {gDevTools} = require("devtools/client/framework/devtools");
const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");
const {Curl, CurlUtils} = require("devtools/client/shared/curl");
const {PluralForm} = require("devtools/shared/plural-form");
const {Filters, isFreetextMatch} = require("./filter-predicates");
const {Sorters} = require("./sort-predicates");
const {L10N, WEBCONSOLE_L10N} = require("./l10n");
const {getFormDataSections,
       formDataURI,
       writeHeaderText,
       getKeyWithEvent,
       getAbbreviatedMimeType,
       getUriNameWithQuery,
       getUriHostPort,
       getUriHost,
       loadCauseString} = require("./request-utils");
const Actions = require("./actions/index");

loader.lazyServiceGetter(this, "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1", "nsIClipboardHelper");

loader.lazyRequireGetter(this, "HarExporter",
  "devtools/client/netmonitor/har/har-exporter", true);

loader.lazyRequireGetter(this, "NetworkHelper",
  "devtools/shared/webconsole/network-helper");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const EPSILON = 0.001;
// ms
const RESIZE_REFRESH_RATE = 50;
// ms
const REQUESTS_REFRESH_RATE = 50;
// tooltip show/hide delay in ms
const REQUESTS_TOOLTIP_TOGGLE_DELAY = 500;
// px
const REQUESTS_TOOLTIP_IMAGE_MAX_DIM = 400;
// px
const REQUESTS_TOOLTIP_STACK_TRACE_WIDTH = 600;
// px
const REQUESTS_WATERFALL_SAFE_BOUNDS = 90;
// ms
const REQUESTS_WATERFALL_HEADER_TICKS_MULTIPLE = 5;
// px
const REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN = 60;
// ms
const REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5;
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES = 3;
// px
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10;
const REQUESTS_WATERFALL_BACKGROUND_TICKS_COLOR_RGB = [128, 136, 144];
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 32;
// byte
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 32;
const REQUESTS_WATERFALL_DOMCONTENTLOADED_TICKS_COLOR_RGBA = [255, 0, 0, 128];
const REQUESTS_WATERFALL_LOAD_TICKS_COLOR_RGBA = [0, 0, 255, 128];

// Constants for formatting bytes.
const BYTES_IN_KB = 1024;
const BYTES_IN_MB = Math.pow(BYTES_IN_KB, 2);
const BYTES_IN_GB = Math.pow(BYTES_IN_KB, 3);
const MAX_BYTES_SIZE = 1000;
const MAX_KB_SIZE = 1000 * BYTES_IN_KB;
const MAX_MB_SIZE = 1000 * BYTES_IN_MB;

// TODO: duplicated from netmonitor-view.js. Move to a format-utils.js module.
const REQUEST_TIME_DECIMALS = 2;
const CONTENT_SIZE_DECIMALS = 2;

const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};

// A smart store watcher to notify store changes as necessary
function storeWatcher(initialValue, reduceValue, onChange) {
  let currentValue = initialValue;

  return () => {
    const newValue = reduceValue(currentValue);
    if (newValue !== currentValue) {
      onChange(newValue, currentValue);
      currentValue = newValue;
    }
  };
}

/**
 * Functions handling the requests menu (containing details about each request,
 * like status, method, file, domain, as well as a waterfall representing
 * timing imformation).
 */
function RequestsMenuView() {
  dumpn("RequestsMenuView was instantiated");

  this._flushRequests = this._flushRequests.bind(this);
  this._onHover = this._onHover.bind(this);
  this._onSelect = this._onSelect.bind(this);
  this._onSwap = this._onSwap.bind(this);
  this._onResize = this._onResize.bind(this);
  this._onScroll = this._onScroll.bind(this);
  this._onSecurityIconClick = this._onSecurityIconClick.bind(this);
}

RequestsMenuView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize: function (store) {
    dumpn("Initializing the RequestsMenuView");

    this.store = store;

    let widgetParentEl = $("#requests-menu-contents");
    this.widget = new SideMenuWidget(widgetParentEl);
    this._splitter = $("#network-inspector-view-splitter");
    this._summary = $("#requests-menu-network-summary-button");
    this._summary.setAttribute("label", L10N.getStr("networkMenu.empty"));

    // Create a tooltip for the newly appended network request item.
    this.tooltip = new HTMLTooltip(NetMonitorController._toolbox.doc, { type: "arrow" });
    this.tooltip.startTogglingOnHover(widgetParentEl, this._onHover, {
      toggleDelay: REQUESTS_TOOLTIP_TOGGLE_DELAY,
      interactive: true
    });

    this.sortContents((a, b) => Sorters.waterfall(a.attachment, b.attachment));

    this.allowFocusOnRightClick = true;
    this.maintainSelectionVisible = true;

    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("swap", this._onSwap, false);
    this._splitter.addEventListener("mousemove", this._onResize, false);
    window.addEventListener("resize", this._onResize, false);

    this.requestsMenuSortEvent = getKeyWithEvent(this.sortBy.bind(this));
    this.requestsMenuSortKeyboardEvent = getKeyWithEvent(this.sortBy.bind(this), true);
    this._onContextMenu = this._onContextMenu.bind(this);
    this._onContextNewTabCommand = this.openRequestInTab.bind(this);
    this._onContextCopyUrlCommand = this.copyUrl.bind(this);
    this._onContextCopyImageAsDataUriCommand =
      this.copyImageAsDataUri.bind(this);
    this._onContextCopyResponseCommand = this.copyResponse.bind(this);
    this._onContextResendCommand = this.cloneSelectedRequest.bind(this);
    this._onContextToggleRawHeadersCommand = this.toggleRawHeaders.bind(this);
    this._onContextPerfCommand = () => NetMonitorView.toggleFrontendMode();
    this._onReloadCommand = () => NetMonitorView.reloadPage();
    this._flushRequestsTask = new DeferredTask(this._flushRequests,
      REQUESTS_REFRESH_RATE);

    this.sendCustomRequestEvent = this.sendCustomRequest.bind(this);
    this.closeCustomRequestEvent = this.closeCustomRequest.bind(this);
    this.cloneSelectedRequestEvent = this.cloneSelectedRequest.bind(this);
    this.toggleRawHeadersEvent = this.toggleRawHeaders.bind(this);

    this.reFilterRequests = this.reFilterRequests.bind(this);

    $("#toolbar-labels").addEventListener("click",
      this.requestsMenuSortEvent, false);
    $("#toolbar-labels").addEventListener("keydown",
      this.requestsMenuSortKeyboardEvent, false);
    $("#toggle-raw-headers").addEventListener("click",
      this.toggleRawHeadersEvent, false);
    $("#requests-menu-contents").addEventListener("scroll", this._onScroll, true);
    $("#requests-menu-contents").addEventListener("contextmenu", this._onContextMenu);

    this.unsubscribeStore = store.subscribe(storeWatcher(
      null,
      () => store.getState().filters,
      (newFilters) => {
        this._activeFilters = newFilters.types
          .toSeq()
          .filter((checked, key) => checked)
          .keySeq()
          .toArray();
        this._currentFreetextFilter = newFilters.url;
        this.reFilterRequests();
      }
    ));

    Prefs.filters.forEach(type =>
      store.dispatch(Actions.toggleFilterType(type)));

    window.once("connected", this._onConnect.bind(this));
  },

  _onConnect: function () {
    $("#requests-menu-reload-notice-button").addEventListener("command",
      this._onReloadCommand, false);

    if (NetMonitorController.supportsCustomRequest) {
      $("#custom-request-send-button").addEventListener("click",
        this.sendCustomRequestEvent, false);
      $("#custom-request-close-button").addEventListener("click",
        this.closeCustomRequestEvent, false);
      $("#headers-summary-resend").addEventListener("click",
        this.cloneSelectedRequestEvent, false);
    } else {
      $("#headers-summary-resend").hidden = true;
    }

    if (NetMonitorController.supportsPerfStats) {
      $("#requests-menu-perf-notice-button").addEventListener("command",
        this._onContextPerfCommand, false);
      $("#requests-menu-network-summary-button").addEventListener("command",
        this._onContextPerfCommand, false);
      $("#network-statistics-back-button").addEventListener("command",
        this._onContextPerfCommand, false);
    } else {
      $("#notice-perf-message").hidden = true;
      $("#requests-menu-network-summary-button").hidden = true;
    }

    if (!NetMonitorController.supportsTransferredResponseSize) {
      $("#requests-menu-transferred-header-box").hidden = true;
      $("#requests-menu-item-template .requests-menu-transferred")
        .hidden = true;
    }
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy: function () {
    dumpn("Destroying the RequestsMenuView");

    Prefs.filters = this._activeFilters;

    /* Destroy the tooltip */
    this.tooltip.stopTogglingOnHover();
    this.tooltip.destroy();
    $("#requests-menu-contents").removeEventListener("scroll", this._onScroll, true);
    $("#requests-menu-contents").removeEventListener("contextmenu", this._onContextMenu);

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("swap", this._onSwap, false);
    this._splitter.removeEventListener("mousemove", this._onResize, false);
    window.removeEventListener("resize", this._onResize, false);

    $("#toolbar-labels").removeEventListener("click",
      this.requestsMenuSortEvent, false);
    $("#toolbar-labels").removeEventListener("keydown",
      this.requestsMenuSortKeyboardEvent, false);

    this._flushRequestsTask.disarm();

    $("#requests-menu-reload-notice-button").removeEventListener("command",
      this._onReloadCommand, false);
    $("#requests-menu-perf-notice-button").removeEventListener("command",
      this._onContextPerfCommand, false);
    $("#requests-menu-network-summary-button").removeEventListener("command",
      this._onContextPerfCommand, false);
    $("#network-statistics-back-button").removeEventListener("command",
      this._onContextPerfCommand, false);

    $("#custom-request-send-button").removeEventListener("click",
      this.sendCustomRequestEvent, false);
    $("#custom-request-close-button").removeEventListener("click",
      this.closeCustomRequestEvent, false);
    $("#headers-summary-resend").removeEventListener("click",
      this.cloneSelectedRequestEvent, false);
    $("#toggle-raw-headers").removeEventListener("click",
      this.toggleRawHeadersEvent, false);

    this.unsubscribeStore();
  },

  /**
   * Resets this container (removes all the networking information).
   */
  reset: function () {
    this.empty();
    this._addQueue = [];
    this._updateQueue = [];
    this._firstRequestStartedMillis = -1;
    this._lastRequestEndedMillis = -1;
  },

  /**
   * Specifies if this view may be updated lazily.
   */
  _lazyUpdate: true,

  get lazyUpdate() {
    return this._lazyUpdate;
  },

  set lazyUpdate(value) {
    this._lazyUpdate = value;
    if (!value) {
      this._flushRequests();
    }
  },

  /**
   * Adds a network request to this container.
   *
   * @param string id
   *        An identifier coming from the network monitor controller.
   * @param string startedDateTime
   *        A string representation of when the request was started, which
   *        can be parsed by Date (for example "2012-09-17T19:50:03.699Z").
   * @param string method
   *        Specifies the request method (e.g. "GET", "POST", etc.)
   * @param string url
   *        Specifies the request's url.
   * @param boolean isXHR
   *        True if this request was initiated via XHR.
   * @param object cause
   *        Specifies the request's cause. Has the following properties:
   *        - type: nsContentPolicyType constant
   *        - loadingDocumentUri: URI of the request origin
   *        - stacktrace: JS stacktrace of the request
   * @param boolean fromCache
   *        Indicates if the result came from the browser cache
   * @param boolean fromServiceWorker
   *        Indicates if the request has been intercepted by a Service Worker
   */
  addRequest: function (id, startedDateTime, method, url, isXHR, cause,
    fromCache, fromServiceWorker) {
    this._addQueue.push([id, startedDateTime, method, url, isXHR, cause,
      fromCache, fromServiceWorker]);

    // Lazy updating is disabled in some tests.
    if (!this.lazyUpdate) {
      return void this._flushRequests();
    }

    this._flushRequestsTask.arm();
    return undefined;
  },

  /**
   * Opens selected item in a new tab.
   */
  openRequestInTab: function () {
    let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
    let selected = this.selectedItem.attachment;
    win.openUILinkIn(selected.url, "tab", { relatedToCurrent: true });
  },

  /**
   * Copy the request url from the currently selected item.
   */
  copyUrl: function () {
    let selected = this.selectedItem.attachment;
    clipboardHelper.copyString(selected.url);
  },

  /**
   * Copy the request url query string parameters from the currently
   * selected item.
   */
  copyUrlParams: function () {
    let selected = this.selectedItem.attachment;
    let params = NetworkHelper.nsIURL(selected.url).query.split("&");
    let string = params.join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n");
    clipboardHelper.copyString(string);
  },

  /**
   * Copy the request form data parameters (or raw payload) from
   * the currently selected item.
   */
  copyPostData: Task.async(function* () {
    let selected = this.selectedItem.attachment;

    // Try to extract any form data parameters.
    let formDataSections = yield getFormDataSections(
      selected.requestHeaders,
      selected.requestHeadersFromUploadStream,
      selected.requestPostData,
      gNetwork.getString.bind(gNetwork));

    let params = [];
    formDataSections.forEach(section => {
      let paramsArray = NetworkHelper.parseQueryString(section);
      if (paramsArray) {
        params = [...params, ...paramsArray];
      }
    });

    let string = params
      .map(param => param.name + (param.value ? "=" + param.value : ""))
      .join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n");

    // Fall back to raw payload.
    if (!string) {
      let postData = selected.requestPostData.postData.text;
      string = yield gNetwork.getString(postData);
      if (Services.appinfo.OS !== "WINNT") {
        string = string.replace(/\r/g, "");
      }
    }

    clipboardHelper.copyString(string);
  }),

  /**
   * Copy a cURL command from the currently selected item.
   */
  copyAsCurl: function () {
    let selected = this.selectedItem.attachment;

    Task.spawn(function* () {
      // Create a sanitized object for the Curl command generator.
      let data = {
        url: selected.url,
        method: selected.method,
        headers: [],
        httpVersion: selected.httpVersion,
        postDataText: null
      };

      // Fetch header values.
      for (let { name, value } of selected.requestHeaders.headers) {
        let text = yield gNetwork.getString(value);
        data.headers.push({ name: name, value: text });
      }

      // Fetch the request payload.
      if (selected.requestPostData) {
        let postData = selected.requestPostData.postData.text;
        data.postDataText = yield gNetwork.getString(postData);
      }

      clipboardHelper.copyString(Curl.generateCommand(data));
    });
  },

  /**
   * Copy HAR from the network panel content to the clipboard.
   */
  copyAllAsHar: function () {
    let options = this.getDefaultHarOptions();
    return HarExporter.copy(options);
  },

  /**
   * Save HAR from the network panel content to a file.
   */
  saveAllAsHar: function () {
    let options = this.getDefaultHarOptions();
    return HarExporter.save(options);
  },

  getDefaultHarOptions: function () {
    let form = NetMonitorController._target.form;
    let title = form.title || form.url;

    return {
      getString: gNetwork.getString.bind(gNetwork),
      view: this,
      items: NetMonitorView.RequestsMenu.items,
      title: title
    };
  },

  /**
   * Copy the raw request headers from the currently selected item.
   */
  copyRequestHeaders: function () {
    let selected = this.selectedItem.attachment;
    let rawHeaders = selected.requestHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    clipboardHelper.copyString(rawHeaders);
  },

  /**
   * Copy the raw response headers from the currently selected item.
   */
  copyResponseHeaders: function () {
    let selected = this.selectedItem.attachment;
    let rawHeaders = selected.responseHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    clipboardHelper.copyString(rawHeaders);
  },

  /**
   * Copy image as data uri.
   */
  copyImageAsDataUri: function () {
    let selected = this.selectedItem.attachment;
    let { mimeType, text, encoding } = selected.responseContent.content;

    gNetwork.getString(text).then(string => {
      let data = formDataURI(mimeType, encoding, string);
      clipboardHelper.copyString(data);
    });
  },

  /**
   * Copy response data as a string.
   */
  copyResponse: function () {
    let selected = this.selectedItem.attachment;
    let text = selected.responseContent.content.text;

    gNetwork.getString(text).then(string => {
      clipboardHelper.copyString(string);
    });
  },

  /**
   * Create a new custom request form populated with the data from
   * the currently selected request.
   */
  cloneSelectedRequest: function () {
    let selected = this.selectedItem.attachment;

    // Create the element node for the network request item.
    let menuView = this._createMenuView(selected.method, selected.url,
      selected.cause);

    // Append a network request item to this container.
    let newItem = this.push([menuView], {
      attachment: Object.create(selected, {
        isCustom: { value: true }
      })
    });

    // Immediately switch to new request pane.
    this.selectedItem = newItem;
  },

  /**
   * Send a new HTTP request using the data in the custom request form.
   */
  sendCustomRequest: function () {
    let selected = this.selectedItem.attachment;

    let data = {
      url: selected.url,
      method: selected.method,
      httpVersion: selected.httpVersion,
    };
    if (selected.requestHeaders) {
      data.headers = selected.requestHeaders.headers;
    }
    if (selected.requestPostData) {
      data.body = selected.requestPostData.postData.text;
    }

    NetMonitorController.webConsoleClient.sendHTTPRequest(data, response => {
      let id = response.eventActor.actor;
      this._preferredItemId = id;
    });

    this.closeCustomRequest();
  },

  /**
   * Remove the currently selected custom request.
   */
  closeCustomRequest: function () {
    this.remove(this.selectedItem);
    NetMonitorView.Sidebar.toggle(false);
  },

  /**
   * Shows raw request/response headers in textboxes.
   */
  toggleRawHeaders: function () {
    let requestTextarea = $("#raw-request-headers-textarea");
    let responseTextare = $("#raw-response-headers-textarea");
    let rawHeadersHidden = $("#raw-headers").getAttribute("hidden");

    if (rawHeadersHidden) {
      let selected = this.selectedItem.attachment;
      let selectedRequestHeaders = selected.requestHeaders.headers;
      let selectedResponseHeaders = selected.responseHeaders.headers;
      requestTextarea.value = writeHeaderText(selectedRequestHeaders);
      responseTextare.value = writeHeaderText(selectedResponseHeaders);
      $("#raw-headers").hidden = false;
    } else {
      requestTextarea.value = null;
      responseTextare.value = null;
      $("#raw-headers").hidden = true;
    }
  },

  /**
   * Refreshes the view contents with the newly selected filters
   */
  reFilterRequests: function () {
    this.filterContents(this._filterPredicate);
    this.refreshSummary();
    this.refreshZebra();
  },

  /**
   * Returns a predicate that can be used to test if a request matches any of
   * the active filters.
   */
  get _filterPredicate() {
    let currentFreetextFilter = this._currentFreetextFilter;

    return requestItem => {
      const { attachment } = requestItem;
      return this._activeFilters.some(filterName => Filters[filterName](attachment)) &&
          isFreetextMatch(attachment, currentFreetextFilter);
    };
  },

  /**
   * Sorts all network requests in this container by a specified detail.
   *
   * @param string type
   *        Either "status", "method", "file", "domain", "type", "transferred",
   *        "size" or "waterfall".
   */
  sortBy: function (type = "waterfall") {
    let target = $("#requests-menu-" + type + "-button");
    let headers = document.querySelectorAll(".requests-menu-header-button");

    for (let header of headers) {
      if (header != target) {
        header.removeAttribute("sorted");
        header.removeAttribute("tooltiptext");
        header.parentNode.removeAttribute("active");
      }
    }

    let direction = "";
    if (target) {
      if (target.getAttribute("sorted") == "ascending") {
        target.setAttribute("sorted", direction = "descending");
        target.setAttribute("tooltiptext",
          L10N.getStr("networkMenu.sortedDesc"));
      } else {
        target.setAttribute("sorted", direction = "ascending");
        target.setAttribute("tooltiptext",
          L10N.getStr("networkMenu.sortedAsc"));
      }
      // Used to style the next column.
      target.parentNode.setAttribute("active", "true");
    }

    // Sort by whatever was requested.
    switch (type) {
      case "status":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.status(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.status(a.attachment, b.attachment));
        }
        break;
      case "method":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.method(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.method(a.attachment, b.attachment));
        }
        break;
      case "file":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.file(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.file(a.attachment, b.attachment));
        }
        break;
      case "domain":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.domain(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.domain(a.attachment, b.attachment));
        }
        break;
      case "cause":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.cause(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.cause(a.attachment, b.attachment));
        }
        break;
      case "type":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.type(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.type(a.attachment, b.attachment));
        }
        break;
      case "transferred":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.transferred(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.transferred(a.attachment, b.attachment));
        }
        break;
      case "size":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.size(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.size(a.attachment, b.attachment));
        }
        break;
      case "waterfall":
        if (direction == "ascending") {
          this.sortContents((a, b) => Sorters.waterfall(a.attachment, b.attachment));
        } else {
          this.sortContents((a, b) => -Sorters.waterfall(a.attachment, b.attachment));
        }
        break;
    }

    this.refreshSummary();
    this.refreshZebra();
  },

  /**
   * Removes all network requests and closes the sidebar if open.
   */
  clear: function () {
    NetMonitorController.NetworkEventsHandler.clearMarkers();
    NetMonitorView.Sidebar.toggle(false);

    this.store.dispatch(Actions.disableToggleButton(true));
    $("#requests-menu-empty-notice").hidden = false;

    this.empty();
    this.refreshSummary();
  },

  /**
   * Refreshes the status displayed in this container's footer, providing
   * concise information about all requests.
   */
  refreshSummary: function () {
    let visibleItems = this.visibleItems;
    let visibleRequestsCount = visibleItems.length;
    if (!visibleRequestsCount) {
      this._summary.setAttribute("label", L10N.getStr("networkMenu.empty"));
      return;
    }

    let totalBytes = this._getTotalBytesOfRequests(visibleItems);
    let totalMillis =
      this._getNewestRequest(visibleItems).attachment.endedMillis -
      this._getOldestRequest(visibleItems).attachment.startedMillis;

    // https://developer.mozilla.org/en-US/docs/Localization_and_Plurals
    let str = PluralForm.get(visibleRequestsCount,
      L10N.getStr("networkMenu.summary"));

    this._summary.setAttribute("label", str
      .replace("#1", visibleRequestsCount)
      .replace("#2", L10N.numberWithDecimals((totalBytes || 0) / 1024,
        CONTENT_SIZE_DECIMALS))
      .replace("#3", L10N.numberWithDecimals((totalMillis || 0) / 1000,
        REQUEST_TIME_DECIMALS))
    );
  },

  /**
   * Adds odd/even attributes to all the visible items in this container.
   */
  refreshZebra: function () {
    let visibleItems = this.visibleItems;

    for (let i = 0, len = visibleItems.length; i < len; i++) {
      let requestItem = visibleItems[i];
      let requestTarget = requestItem.target;

      if (i % 2 == 0) {
        requestTarget.setAttribute("even", "");
        requestTarget.removeAttribute("odd");
      } else {
        requestTarget.setAttribute("odd", "");
        requestTarget.removeAttribute("even");
      }
    }
  },

  /**
   * Attaches security icon click listener for the given request menu item.
   *
   * @param object item
   *        The network request item to attach the listener to.
   */
  attachSecurityIconClickListener: function ({ target }) {
    let icon = $(".requests-security-state-icon", target);
    icon.addEventListener("click", this._onSecurityIconClick);
  },

  /**
   * Schedules adding additional information to a network request.
   *
   * @param string id
   *        An identifier coming from the network monitor controller.
   * @param object data
   *        An object containing several { key: value } tuples of network info.
   *        Supported keys are "httpVersion", "status", "statusText" etc.
   * @param function callback
   *        A function to call once the request has been updated in the view.
   */
  updateRequest: function (id, data, callback) {
    this._updateQueue.push([id, data, callback]);

    // Lazy updating is disabled in some tests.
    if (!this.lazyUpdate) {
      return void this._flushRequests();
    }

    this._flushRequestsTask.arm();
    return undefined;
  },

  /**
   * Starts adding all queued additional information about network requests.
   */
  _flushRequests: function () {
    // Prevent displaying any updates received after the target closed.
    if (NetMonitorView._isDestroyed) {
      return;
    }

    let widget = NetMonitorView.RequestsMenu.widget;
    let isScrolledToBottom = widget.isScrolledToBottom();

    for (let [id, startedDateTime, method, url, isXHR, cause, fromCache,
      fromServiceWorker] of this._addQueue) {
      // Convert the received date/time string to a unix timestamp.
      let unixTime = Date.parse(startedDateTime);

      // Create the element node for the network request item.
      let menuView = this._createMenuView(method, url, cause);

      // Remember the first and last event boundaries.
      this._registerFirstRequestStart(unixTime);
      this._registerLastRequestEnd(unixTime);

      // Append a network request item to this container.
      let requestItem = this.push([menuView, id], {
        attachment: {
          startedDeltaMillis: unixTime - this._firstRequestStartedMillis,
          startedMillis: unixTime,
          method: method,
          url: url,
          isXHR: isXHR,
          cause: cause,
          fromCache: fromCache,
          fromServiceWorker: fromServiceWorker
        }
      });

      if (id == this._preferredItemId) {
        this.selectedItem = requestItem;
      }

      window.emit(EVENTS.REQUEST_ADDED, id);
    }

    if (isScrolledToBottom && this._addQueue.length) {
      widget.scrollToBottom();
    }

    // For each queued additional information packet, get the corresponding
    // request item in the view and update it based on the specified data.
    for (let [id, data, callback] of this._updateQueue) {
      let requestItem = this.getItemByValue(id);
      if (!requestItem) {
        // Packet corresponds to a dead request item, target navigated.
        continue;
      }

      // Each information packet may contain several { key: value } tuples of
      // network info, so update the view based on each one.
      for (let key in data) {
        let val = data[key];
        if (val === undefined) {
          // The information in the packet is empty, it can be safely ignored.
          continue;
        }

        switch (key) {
          case "requestHeaders":
            requestItem.attachment.requestHeaders = val;
            break;
          case "requestCookies":
            requestItem.attachment.requestCookies = val;
            break;
          case "requestPostData":
            // Search the POST data upload stream for request headers and add
            // them to a separate store, different from the classic headers.
            // XXX: Be really careful here! We're creating a function inside
            // a loop, so remember the actual request item we want to modify.
            let currentItem = requestItem;
            let currentStore = { headers: [], headersSize: 0 };

            Task.spawn(function* () {
              let postData = yield gNetwork.getString(val.postData.text);
              let payloadHeaders = CurlUtils.getHeadersFromMultipartText(
                postData);

              currentStore.headers = payloadHeaders;
              currentStore.headersSize = payloadHeaders.reduce(
                (acc, { name, value }) =>
                  acc + name.length + value.length + 2, 0);

              // The `getString` promise is async, so we need to refresh the
              // information displayed in the network details pane again here.
              refreshNetworkDetailsPaneIfNecessary(currentItem);
            });

            requestItem.attachment.requestPostData = val;
            requestItem.attachment.requestHeadersFromUploadStream =
              currentStore;
            break;
          case "securityState":
            requestItem.attachment.securityState = val;
            this.updateMenuView(requestItem, key, val);
            break;
          case "securityInfo":
            requestItem.attachment.securityInfo = val;
            break;
          case "responseHeaders":
            requestItem.attachment.responseHeaders = val;
            break;
          case "responseCookies":
            requestItem.attachment.responseCookies = val;
            break;
          case "httpVersion":
            requestItem.attachment.httpVersion = val;
            break;
          case "remoteAddress":
            requestItem.attachment.remoteAddress = val;
            this.updateMenuView(requestItem, key, val);
            break;
          case "remotePort":
            requestItem.attachment.remotePort = val;
            break;
          case "status":
            requestItem.attachment.status = val;
            this.updateMenuView(requestItem, key, {
              status: val,
              cached: requestItem.attachment.fromCache,
              serviceWorker: requestItem.attachment.fromServiceWorker
            });
            break;
          case "statusText":
            requestItem.attachment.statusText = val;
            let text = (requestItem.attachment.status + " " +
                        requestItem.attachment.statusText);
            if (requestItem.attachment.fromCache) {
              text += " (cached)";
            } else if (requestItem.attachment.fromServiceWorker) {
              text += " (service worker)";
            }

            this.updateMenuView(requestItem, key, text);
            break;
          case "headersSize":
            requestItem.attachment.headersSize = val;
            break;
          case "contentSize":
            requestItem.attachment.contentSize = val;
            this.updateMenuView(requestItem, key, val);
            break;
          case "transferredSize":
            if (requestItem.attachment.fromCache) {
              requestItem.attachment.transferredSize = 0;
              this.updateMenuView(requestItem, key, "cached");
            } else if (requestItem.attachment.fromServiceWorker) {
              requestItem.attachment.transferredSize = 0;
              this.updateMenuView(requestItem, key, "service worker");
            } else {
              requestItem.attachment.transferredSize = val;
              this.updateMenuView(requestItem, key, val);
            }
            break;
          case "mimeType":
            requestItem.attachment.mimeType = val;
            this.updateMenuView(requestItem, key, val);
            break;
          case "responseContent":
            // If there's no mime type available when the response content
            // is received, assume text/plain as a fallback.
            if (!requestItem.attachment.mimeType) {
              requestItem.attachment.mimeType = "text/plain";
              this.updateMenuView(requestItem, "mimeType", "text/plain");
            }
            requestItem.attachment.responseContent = val;
            this.updateMenuView(requestItem, key, val);
            break;
          case "totalTime":
            requestItem.attachment.totalTime = val;
            requestItem.attachment.endedMillis =
              requestItem.attachment.startedMillis + val;

            this.updateMenuView(requestItem, key, val);
            this._registerLastRequestEnd(requestItem.attachment.endedMillis);
            break;
          case "eventTimings":
            requestItem.attachment.eventTimings = val;
            this._createWaterfallView(
              requestItem, val.timings,
              requestItem.attachment.fromCache ||
              requestItem.attachment.fromServiceWorker
            );
            break;
        }
      }
      refreshNetworkDetailsPaneIfNecessary(requestItem);

      if (callback) {
        callback();
      }
    }

    /**
     * Refreshes the information displayed in the sidebar, in case this update
     * may have additional information about a request which isn't shown yet
     * in the network details pane.
     *
     * @param object requestItem
     *        The item to repopulate the sidebar with in case it's selected in
     *        this requests menu.
     */
    function refreshNetworkDetailsPaneIfNecessary(requestItem) {
      let selectedItem = NetMonitorView.RequestsMenu.selectedItem;
      if (selectedItem == requestItem) {
        NetMonitorView.NetworkDetails.populate(selectedItem.attachment);
      }
    }

    // We're done flushing all the requests, clear the update queue.
    this._updateQueue = [];
    this._addQueue = [];

    this.store.dispatch(Actions.disableToggleButton(!this.itemCount));
    $("#requests-menu-empty-notice").hidden = !!this.itemCount;

    // Make sure all the requests are sorted and filtered.
    // Freshly added requests may not yet contain all the information required
    // for sorting and filtering predicates, so this is done each time the
    // network requests table is flushed (don't worry, events are drained first
    // so this doesn't happen once per network event update).
    this.sortContents();
    this.filterContents();
    this.refreshSummary();
    this.refreshZebra();

    // Rescale all the waterfalls so that everything is visible at once.
    this._flushWaterfallViews();
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string method
   *        Specifies the request method (e.g. "GET", "POST", etc.)
   * @param string url
   *        Specifies the request's url.
   * @param object cause
   *        Specifies the request's cause. Has two properties:
   *        - type: nsContentPolicyType constant
   *        - uri: URI of the request origin
   * @return nsIDOMNode
   *         The network request view.
   */
  _createMenuView: function (method, url, cause) {
    let template = $("#requests-menu-item-template");
    let fragment = document.createDocumentFragment();

    // Flatten the DOM by removing one redundant box (the template container).
    for (let node of template.childNodes) {
      fragment.appendChild(node.cloneNode(true));
    }

    this.updateMenuView(fragment, "method", method);
    this.updateMenuView(fragment, "url", url);
    this.updateMenuView(fragment, "cause", cause);

    return fragment;
  },

  /**
   * Get a human-readable string from a number of bytes, with the B, KB, MB, or
   * GB value. Note that the transition between abbreviations is by 1000 rather
   * than 1024 in order to keep the displayed digits smaller as "1016 KB" is
   * more awkward than 0.99 MB"
   */
  getFormattedSize(bytes) {
    if (bytes < MAX_BYTES_SIZE) {
      return L10N.getFormatStr("networkMenu.sizeB", bytes);
    } else if (bytes < MAX_KB_SIZE) {
      let kb = bytes / BYTES_IN_KB;
      let size = L10N.numberWithDecimals(kb, CONTENT_SIZE_DECIMALS);
      return L10N.getFormatStr("networkMenu.sizeKB", size);
    } else if (bytes < MAX_MB_SIZE) {
      let mb = bytes / BYTES_IN_MB;
      let size = L10N.numberWithDecimals(mb, CONTENT_SIZE_DECIMALS);
      return L10N.getFormatStr("networkMenu.sizeMB", size);
    }
    let gb = bytes / BYTES_IN_GB;
    let size = L10N.numberWithDecimals(gb, CONTENT_SIZE_DECIMALS);
    return L10N.getFormatStr("networkMenu.sizeGB", size);
  },

  /**
   * Updates the information displayed in a network request item view.
   *
   * @param object item
   *        The network request item in this container.
   * @param string key
   *        The type of information that is to be updated.
   * @param any value
   *        The new value to be shown.
   * @return object
   *         A promise that is resolved once the information is displayed.
   */
  updateMenuView: Task.async(function* (item, key, value) {
    let target = item.target || item;

    switch (key) {
      case "method": {
        let node = $(".requests-menu-method", target);
        node.setAttribute("value", value);
        break;
      }
      case "url": {
        let uri;
        try {
          uri = NetworkHelper.nsIURL(value);
        } catch (e) {
          // User input may not make a well-formed url yet.
          break;
        }
        let nameWithQuery = getUriNameWithQuery(uri);
        let hostPort = getUriHostPort(uri);
        let host = getUriHost(uri);
        let unicodeUrl = NetworkHelper.convertToUnicode(unescape(uri.spec));

        let file = $(".requests-menu-file", target);
        file.setAttribute("value", nameWithQuery);
        file.setAttribute("tooltiptext", unicodeUrl);

        let domain = $(".requests-menu-domain", target);
        domain.setAttribute("value", hostPort);
        domain.setAttribute("tooltiptext", hostPort);

        // Mark local hosts specially, where "local" is  as defined in the W3C
        // spec for secure contexts.
        // http://www.w3.org/TR/powerful-features/
        //
        //  * If the name falls under 'localhost'
        //  * If the name is an IPv4 address within 127.0.0.0/8
        //  * If the name is an IPv6 address within ::1/128
        //
        // IPv6 parsing is a little sloppy; it assumes that the address has
        // been validated before it gets here.
        let icon = $(".requests-security-state-icon", target);
        icon.classList.remove("security-state-local");
        if (host.match(/(.+\.)?localhost$/) ||
            host.match(/^127\.\d{1,3}\.\d{1,3}\.\d{1,3}/) ||
            host.match(/\[[0:]+1\]/)) {
          let tooltip = L10N.getStr("netmonitor.security.state.secure");
          icon.classList.add("security-state-local");
          icon.setAttribute("tooltiptext", tooltip);
        }

        break;
      }
      case "remoteAddress":
        let domain = $(".requests-menu-domain", target);
        let tooltip = (domain.getAttribute("value") +
                       (value ? " (" + value + ")" : ""));
        domain.setAttribute("tooltiptext", tooltip);
        break;
      case "securityState": {
        let icon = $(".requests-security-state-icon", target);
        this.attachSecurityIconClickListener(item);

        // Security icon for local hosts is set in the "url" branch
        if (icon.classList.contains("security-state-local")) {
          break;
        }

        let tooltip2 = L10N.getStr("netmonitor.security.state." + value);
        icon.classList.add("security-state-" + value);
        icon.setAttribute("tooltiptext", tooltip2);
        break;
      }
      case "status": {
        let node = $(".requests-menu-status-icon", target);
        // "code" attribute is only used by css to determine the icon color
        let code;
        if (value.cached) {
          code = "cached";
        } else if (value.serviceWorker) {
          code = "service worker";
        } else {
          code = value.status;
        }
        node.setAttribute("code", code);
        let codeNode = $(".requests-menu-status-code", target);
        codeNode.setAttribute("value", value.status);
        break;
      }
      case "statusText": {
        let node = $(".requests-menu-status", target);
        node.setAttribute("tooltiptext", value);
        break;
      }
      case "cause": {
        let labelNode = $(".requests-menu-cause-label", target);
        labelNode.setAttribute("value", loadCauseString(value.type));
        if (value.loadingDocumentUri) {
          labelNode.setAttribute("tooltiptext", value.loadingDocumentUri);
        }

        let stackNode = $(".requests-menu-cause-stack", target);
        if (value.stacktrace && value.stacktrace.length > 0) {
          stackNode.removeAttribute("hidden");
        }
        break;
      }
      case "contentSize": {
        let node = $(".requests-menu-size", target);

        let text = this.getFormattedSize(value);

        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", text);
        break;
      }
      case "transferredSize": {
        let node = $(".requests-menu-transferred", target);

        let text;
        if (value === null) {
          text = L10N.getStr("networkMenu.sizeUnavailable");
        } else if (value === "cached") {
          text = L10N.getStr("networkMenu.sizeCached");
          node.classList.add("theme-comment");
        } else if (value === "service worker") {
          text = L10N.getStr("networkMenu.sizeServiceWorker");
          node.classList.add("theme-comment");
        } else {
          text = this.getFormattedSize(value);
        }

        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", text);
        break;
      }
      case "mimeType": {
        let type = getAbbreviatedMimeType(value);
        let node = $(".requests-menu-type", target);
        let text = CONTENT_MIME_TYPE_ABBREVIATIONS[type] || type;
        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", value);
        break;
      }
      case "responseContent": {
        let { mimeType } = item.attachment;

        if (mimeType.includes("image/")) {
          let { text, encoding } = value.content;
          let responseBody = yield gNetwork.getString(text);
          let node = $(".requests-menu-icon", item.target);
          node.src = formDataURI(mimeType, encoding, responseBody);
          node.setAttribute("type", "thumbnail");
          node.removeAttribute("hidden");

          window.emit(EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);
        }
        break;
      }
      case "totalTime": {
        let node = $(".requests-menu-timings-total", target);

        // integer
        let text = L10N.getFormatStr("networkMenu.totalMS", value);
        node.setAttribute("value", text);
        node.setAttribute("tooltiptext", text);
        break;
      }
    }
  }),

  /**
   * Creates a waterfall representing timing information in a network
   * request item view.
   *
   * @param object item
   *        The network request item in this container.
   * @param object timings
   *        An object containing timing information.
   * @param boolean fromCache
   *        Indicates if the result came from the browser cache or
   *        a service worker
   */
  _createWaterfallView: function (item, timings, fromCache) {
    let { target } = item;
    let sections = ["blocked", "dns", "connect", "send", "wait", "receive"];
    // Skipping "blocked" because it doesn't work yet.

    let timingsNode = $(".requests-menu-timings", target);
    let timingsTotal = $(".requests-menu-timings-total", timingsNode);

    if (fromCache) {
      timingsTotal.style.display = "none";
      return;
    }

    // Add a set of boxes representing timing information.
    for (let key of sections) {
      let width = timings[key];

      // Don't render anything if it surely won't be visible.
      // One millisecond == one unscaled pixel.
      if (width > 0) {
        let timingBox = document.createElement("hbox");
        timingBox.className = "requests-menu-timings-box " + key;
        timingBox.setAttribute("width", width);
        timingsNode.insertBefore(timingBox, timingsTotal);
      }
    }
  },

  /**
   * Rescales and redraws all the waterfall views in this container.
   *
   * @param boolean reset
   *        True if this container's width was changed.
   */
  _flushWaterfallViews: function (reset) {
    // Don't paint things while the waterfall view isn't even visible,
    // or there are no items added to this container.
    if (NetMonitorView.currentFrontendMode !=
      "network-inspector-view" || !this.itemCount) {
      return;
    }

    // To avoid expensive operations like getBoundingClientRect() and
    // rebuilding the waterfall background each time a new request comes in,
    // stuff is cached. However, in certain scenarios like when the window
    // is resized, this needs to be invalidated.
    if (reset) {
      this._cachedWaterfallWidth = 0;
    }

    // Determine the scaling to be applied to all the waterfalls so that
    // everything is visible at once. One millisecond == one unscaled pixel.
    let availableWidth = this._waterfallWidth - REQUESTS_WATERFALL_SAFE_BOUNDS;
    let longestWidth = this._lastRequestEndedMillis -
      this._firstRequestStartedMillis;
    let scale = Math.min(Math.max(availableWidth / longestWidth, EPSILON), 1);

    // Redraw and set the canvas background for each waterfall view.
    this._showWaterfallDivisionLabels(scale);
    this._drawWaterfallBackground(scale);

    // Apply CSS transforms to each waterfall in this container totalTime
    // accurately translate and resize as needed.
    for (let { target, attachment } of this) {
      let timingsNode = $(".requests-menu-timings", target);
      let totalNode = $(".requests-menu-timings-total", target);
      let direction = window.isRTL ? -1 : 1;

      // Render the timing information at a specific horizontal translation
      // based on the delta to the first monitored event network.
      let translateX = "translateX(" + (direction *
        attachment.startedDeltaMillis) + "px)";

      // Based on the total time passed until the last request, rescale
      // all the waterfalls to a reasonable size.
      let scaleX = "scaleX(" + scale + ")";

      // Certain nodes should not be scaled, even if they're children of
      // another scaled node. In this case, apply a reversed transformation.
      let revScaleX = "scaleX(" + (1 / scale) + ")";

      timingsNode.style.transform = scaleX + " " + translateX;
      totalNode.style.transform = revScaleX;
    }
  },

  /**
   * Creates the labels displayed on the waterfall header in this container.
   *
   * @param number scale
   *        The current waterfall scale.
   */
  _showWaterfallDivisionLabels: function (scale) {
    let container = $("#requests-menu-waterfall-label-wrapper");
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
      let scaledStep = scale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;

      // Insert one label for each division on the current scale.
      let fragment = document.createDocumentFragment();
      let direction = window.isRTL ? -1 : 1;

      for (let x = 0; x < availableWidth; x += scaledStep) {
        let translateX = "translateX(" + ((direction * x) | 0) + "px)";
        let millisecondTime = x / scale;

        let normalizedTime = millisecondTime;
        let divisionScale = "millisecond";

        // If the division is greater than 1 minute.
        if (normalizedTime > 60000) {
          normalizedTime /= 60000;
          divisionScale = "minute";
        } else if (normalizedTime > 1000) {
          // If the division is greater than 1 second.
          normalizedTime /= 1000;
          divisionScale = "second";
        }

        // Showing too many decimals is bad UX.
        if (divisionScale == "millisecond") {
          normalizedTime |= 0;
        } else {
          normalizedTime = L10N.numberWithDecimals(normalizedTime,
            REQUEST_TIME_DECIMALS);
        }

        let node = document.createElement("label");
        let text = L10N.getFormatStr("networkMenu." +
          divisionScale, normalizedTime);
        node.className = "plain requests-menu-timings-division";
        node.setAttribute("division-scale", divisionScale);
        node.style.transform = translateX;

        node.setAttribute("value", text);
        fragment.appendChild(node);
      }
      container.appendChild(fragment);

      container.className = "requests-menu-waterfall-visible";
    }
  },

  /**
   * Creates the background displayed on each waterfall view in this container.
   *
   * @param number scale
   *        The current waterfall scale.
   */
  _drawWaterfallBackground: function (scale) {
    if (!this._canvas || !this._ctx) {
      this._canvas = document.createElementNS(HTML_NS, "canvas");
      this._ctx = this._canvas.getContext("2d");
    }
    let canvas = this._canvas;
    let ctx = this._ctx;

    // Nuke the context.
    let canvasWidth = canvas.width = this._waterfallWidth;
    // Awww yeah, 1px, repeats on Y axis.
    let canvasHeight = canvas.height = 1;

    // Start over.
    let imageData = ctx.createImageData(canvasWidth, canvasHeight);
    let pixelArray = imageData.data;

    let buf = new ArrayBuffer(pixelArray.length);
    let view8bit = new Uint8ClampedArray(buf);
    let view32bit = new Uint32Array(buf);

    // Build new millisecond tick lines...
    let timingStep = REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE;
    let [r, g, b] = REQUESTS_WATERFALL_BACKGROUND_TICKS_COLOR_RGB;
    let alphaComponent = REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN;
    let optimalTickIntervalFound = false;

    while (!optimalTickIntervalFound) {
      // Ignore any divisions that would end up being too close to each other.
      let scaledStep = scale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;

      // Insert one pixel for each division on each scale.
      for (let i = 1; i <= REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES; i++) {
        let increment = scaledStep * Math.pow(2, i);
        for (let x = 0; x < canvasWidth; x += increment) {
          let position = (window.isRTL ? canvasWidth - x : x) | 0;
          view32bit[position] =
            (alphaComponent << 24) | (b << 16) | (g << 8) | r;
        }
        alphaComponent += REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD;
      }
    }

    {
      let t = NetMonitorController.NetworkEventsHandler
        .firstDocumentDOMContentLoadedTimestamp;

      let delta = Math.floor((t - this._firstRequestStartedMillis) * scale);
      let [r1, g1, b1, a1] =
        REQUESTS_WATERFALL_DOMCONTENTLOADED_TICKS_COLOR_RGBA;
      view32bit[delta] = (a1 << 24) | (r1 << 16) | (g1 << 8) | b1;
    }
    {
      let t = NetMonitorController.NetworkEventsHandler
        .firstDocumentLoadTimestamp;

      let delta = Math.floor((t - this._firstRequestStartedMillis) * scale);
      let [r2, g2, b2, a2] = REQUESTS_WATERFALL_LOAD_TICKS_COLOR_RGBA;
      view32bit[delta] = (a2 << 24) | (r2 << 16) | (g2 << 8) | b2;
    }

    // Flush the image data and cache the waterfall background.
    pixelArray.set(view8bit);
    ctx.putImageData(imageData, 0, 0);
    document.mozSetImageElement("waterfall-background", canvas);
  },

  /**
   * The selection listener for this container.
   */
  _onSelect: function ({ detail: item }) {
    if (item) {
      NetMonitorView.Sidebar.populate(item.attachment);
      NetMonitorView.Sidebar.toggle(true);
    } else {
      NetMonitorView.Sidebar.toggle(false);
    }
  },

  /**
   * The swap listener for this container.
   * Called when two items switch places, when the contents are sorted.
   */
  _onSwap: function ({ detail: [firstItem, secondItem] }) {
    // Reattach click listener to the security icons
    this.attachSecurityIconClickListener(firstItem);
    this.attachSecurityIconClickListener(secondItem);
  },

  /**
   * The predicate used when deciding whether a popup should be shown
   * over a request item or not.
   *
   * @param nsIDOMNode target
   *        The element node currently being hovered.
   * @param object tooltip
   *        The current tooltip instance.
   * @return {Promise}
   */
  _onHover: Task.async(function* (target, tooltip) {
    let requestItem = this.getItemForElement(target);
    if (!requestItem) {
      return false;
    }

    let hovered = requestItem.attachment;
    if (hovered.responseContent && target.closest(".requests-menu-icon-and-file")) {
      return this._setTooltipImageContent(tooltip, requestItem);
    } else if (hovered.cause && target.closest(".requests-menu-cause-stack")) {
      return this._setTooltipStackTraceContent(tooltip, requestItem);
    }

    return false;
  }),

  _setTooltipImageContent: Task.async(function* (tooltip, requestItem) {
    let { mimeType, text, encoding } = requestItem.attachment.responseContent.content;

    if (!mimeType || !mimeType.includes("image/")) {
      return false;
    }

    let string = yield gNetwork.getString(text);
    let src = formDataURI(mimeType, encoding, string);
    let maxDim = REQUESTS_TOOLTIP_IMAGE_MAX_DIM;
    let { naturalWidth, naturalHeight } = yield getImageDimensions(tooltip.doc, src);
    let options = { maxDim, naturalWidth, naturalHeight };
    setImageTooltip(tooltip, tooltip.doc, src, options);

    return $(".requests-menu-icon", requestItem.target);
  }),

  _setTooltipStackTraceContent: Task.async(function* (tooltip, requestItem) {
    let {stacktrace} = requestItem.attachment.cause;

    if (!stacktrace || stacktrace.length == 0) {
      return false;
    }

    let doc = tooltip.doc;
    let el = doc.createElementNS(HTML_NS, "div");
    el.className = "stack-trace-tooltip devtools-monospace";

    for (let f of stacktrace) {
      let { functionName, filename, lineNumber, columnNumber, asyncCause } = f;

      if (asyncCause) {
        // if there is asyncCause, append a "divider" row into the trace
        let asyncFrameEl = doc.createElementNS(HTML_NS, "div");
        asyncFrameEl.className = "stack-frame stack-frame-async";
        asyncFrameEl.textContent =
          WEBCONSOLE_L10N.getFormatStr("stacktrace.asyncStack", asyncCause);
        el.appendChild(asyncFrameEl);
      }

      // Parse a source name in format "url -> url"
      let sourceUrl = filename.split(" -> ").pop();

      let frameEl = doc.createElementNS(HTML_NS, "div");
      frameEl.className = "stack-frame stack-frame-call";

      let funcEl = doc.createElementNS(HTML_NS, "span");
      funcEl.className = "stack-frame-function-name";
      funcEl.textContent =
        functionName || WEBCONSOLE_L10N.getStr("stacktrace.anonymousFunction");
      frameEl.appendChild(funcEl);

      let sourceEl = doc.createElementNS(HTML_NS, "span");
      sourceEl.className = "stack-frame-source-name";
      frameEl.appendChild(sourceEl);

      let sourceInnerEl = doc.createElementNS(HTML_NS, "span");
      sourceInnerEl.className = "stack-frame-source-name-inner";
      sourceEl.appendChild(sourceInnerEl);

      sourceInnerEl.textContent = sourceUrl;
      sourceInnerEl.title = sourceUrl;

      let lineEl = doc.createElementNS(HTML_NS, "span");
      lineEl.className = "stack-frame-line";
      lineEl.textContent = `:${lineNumber}:${columnNumber}`;
      sourceInnerEl.appendChild(lineEl);

      frameEl.addEventListener("click", () => {
        // hide the tooltip immediately, not after delay
        tooltip.hide();
        NetMonitorController.viewSourceInDebugger(filename, lineNumber);
      }, false);

      el.appendChild(frameEl);
    }

    tooltip.setContent(el, {width: REQUESTS_TOOLTIP_STACK_TRACE_WIDTH});

    return true;
  }),

  /**
   * A handler that opens the security tab in the details view if secure or
   * broken security indicator is clicked.
   */
  _onSecurityIconClick: function (e) {
    let state = this.selectedItem.attachment.securityState;
    if (state !== "insecure") {
      // Choose the security tab.
      NetMonitorView.NetworkDetails.widget.selectedIndex = 5;
    }
  },

  /**
   * The resize listener for this container's window.
   */
  _onResize: function (e) {
    // Allow requests to settle down first.
    setNamedTimeout("resize-events",
      RESIZE_REFRESH_RATE, () => this._flushWaterfallViews(true));
  },

  /**
   * Scroll listener for the requests menu view.
   */
  _onScroll: function () {
    this.tooltip.hide();
  },

  /**
   * Open context menu
   */
  _onContextMenu: function (e) {
    e.preventDefault();
    this._openMenu({
      screenX: e.screenX,
      screenY: e.screenY,
      target: e.target,
    });
  },

  /**
   * Handle the context menu opening. Hide items if no request is selected.
   * Since visible attribute only accept boolean value but the method call may
   * return undefined, we use !! to force convert any object to boolean
   */
  _openMenu: function ({ target, screenX = 0, screenY = 0 } = { }) {
    let selectedItem = this.selectedItem;

    let menu = new Menu();
    menu.append(new MenuItem({
      id: "request-menu-context-copy-url",
      label: L10N.getStr("netmonitor.context.copyUrl"),
      accesskey: L10N.getStr("netmonitor.context.copyUrl.accesskey"),
      visible: !!selectedItem,
      click: () => this._onContextCopyUrlCommand(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-url-params",
      label: L10N.getStr("netmonitor.context.copyUrlParams"),
      accesskey: L10N.getStr("netmonitor.context.copyUrlParams.accesskey"),
      visible: !!(selectedItem &&
               NetworkHelper.nsIURL(selectedItem.attachment.url).query),
      click: () => this.copyUrlParams(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-post-data",
      label: L10N.getStr("netmonitor.context.copyPostData"),
      accesskey: L10N.getStr("netmonitor.context.copyPostData.accesskey"),
      visible: !!(selectedItem && selectedItem.attachment.requestPostData),
      click: () => this.copyPostData(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-as-curl",
      label: L10N.getStr("netmonitor.context.copyAsCurl"),
      accesskey: L10N.getStr("netmonitor.context.copyAsCurl.accesskey"),
      visible: !!(selectedItem && selectedItem.attachment),
      click: () => this.copyAsCurl(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedItem,
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-request-headers",
      label: L10N.getStr("netmonitor.context.copyRequestHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyRequestHeaders.accesskey"),
      visible: !!(selectedItem && selectedItem.attachment.requestHeaders),
      click: () => this.copyRequestHeaders(),
    }));

    menu.append(new MenuItem({
      id: "response-menu-context-copy-response-headers",
      label: L10N.getStr("netmonitor.context.copyResponseHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyResponseHeaders.accesskey"),
      visible: !!(selectedItem && selectedItem.attachment.responseHeaders),
      click: () => this.copyResponseHeaders(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-response",
      label: L10N.getStr("netmonitor.context.copyResponse"),
      accesskey: L10N.getStr("netmonitor.context.copyResponse.accesskey"),
      visible: !!(selectedItem &&
               selectedItem.attachment.responseContent &&
               selectedItem.attachment.responseContent.content.text &&
               selectedItem.attachment.responseContent.content.text.length !== 0),
      click: () => this._onContextCopyResponseCommand(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-image-as-data-uri",
      label: L10N.getStr("netmonitor.context.copyImageAsDataUri"),
      accesskey: L10N.getStr("netmonitor.context.copyImageAsDataUri.accesskey"),
      visible: !!(selectedItem &&
               selectedItem.attachment.responseContent &&
               selectedItem.attachment.responseContent.content
                 .mimeType.includes("image/")),
      click: () => this._onContextCopyImageAsDataUriCommand(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedItem,
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      visible: !!this.items.length,
      click: () => this.copyAllAsHar(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-save-all-as-har",
      label: L10N.getStr("netmonitor.context.saveAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
      visible: !!this.items.length,
      click: () => this.saveAllAsHar(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedItem,
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-resend",
      label: L10N.getStr("netmonitor.context.editAndResend"),
      accesskey: L10N.getStr("netmonitor.context.editAndResend.accesskey"),
      visible: !!(NetMonitorController.supportsCustomRequest &&
               selectedItem &&
               !selectedItem.attachment.isCustom),
      click: () => this._onContextResendCommand(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedItem,
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-newtab",
      label: L10N.getStr("netmonitor.context.newTab"),
      accesskey: L10N.getStr("netmonitor.context.newTab.accesskey"),
      visible: !!selectedItem,
      click: () => this._onContextNewTabCommand(),
    }));

    menu.append(new MenuItem({
      id: "request-menu-context-perf",
      label: L10N.getStr("netmonitor.context.perfTools"),
      accesskey: L10N.getStr("netmonitor.context.perfTools.accesskey"),
      visible: !!NetMonitorController.supportsPerfStats,
      click: () => this._onContextPerfCommand(),
    }));

    menu.popup(screenX, screenY, NetMonitorController._toolbox);
    return menu;
  },

  /**
   * Checks if the specified unix time is the first one to be known of,
   * and saves it if so.
   *
   * @param number unixTime
   *        The milliseconds to check and save.
   */
  _registerFirstRequestStart: function (unixTime) {
    if (this._firstRequestStartedMillis == -1) {
      this._firstRequestStartedMillis = unixTime;
    }
  },

  /**
   * Checks if the specified unix time is the last one to be known of,
   * and saves it if so.
   *
   * @param number unixTime
   *        The milliseconds to check and save.
   */
  _registerLastRequestEnd: function (unixTime) {
    if (this._lastRequestEndedMillis < unixTime) {
      this._lastRequestEndedMillis = unixTime;
    }
  },

  /**
   * Gets the total number of bytes representing the cumulated content size of
   * a set of requests. Returns 0 for an empty set.
   *
   * @param array itemsArray
   * @return number
   */
  _getTotalBytesOfRequests: function (itemsArray) {
    if (!itemsArray.length) {
      return 0;
    }

    let result = 0;
    itemsArray.forEach(item => {
      let size = item.attachment.contentSize;
      result += (typeof size == "number") ? size : 0;
    });

    return result;
  },

  /**
   * Gets the oldest (first performed) request in a set. Returns null for an
   * empty set.
   *
   * @param array itemsArray
   * @return object
   */
  _getOldestRequest: function (itemsArray) {
    if (!itemsArray.length) {
      return null;
    }
    return itemsArray.reduce((prev, curr) =>
      prev.attachment.startedMillis < curr.attachment.startedMillis ?
        prev : curr);
  },

  /**
   * Gets the newest (latest performed) request in a set. Returns null for an
   * empty set.
   *
   * @param array itemsArray
   * @return object
   */
  _getNewestRequest: function (itemsArray) {
    if (!itemsArray.length) {
      return null;
    }
    return itemsArray.reduce((prev, curr) =>
      prev.attachment.startedMillis > curr.attachment.startedMillis ?
        prev : curr);
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
      if (!window.isRTL) {
        this._cachedWaterfallWidth = containerBounds.width -
          waterfallBounds.left;
      } else {
        this._cachedWaterfallWidth = waterfallBounds.right;
      }
    }
    return this._cachedWaterfallWidth;
  },

  _splitter: null,
  _summary: null,
  _canvas: null,
  _ctx: null,
  _cachedWaterfallWidth: 0,
  _firstRequestStartedMillis: -1,
  _lastRequestEndedMillis: -1,
  _updateQueue: [],
  _addQueue: [],
  _updateTimeout: null,
  _resizeTimeout: null,
  _activeFilters: ["all"],
  _currentFreetextFilter: ""
});

exports.RequestsMenuView = RequestsMenuView;
