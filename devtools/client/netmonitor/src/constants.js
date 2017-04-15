/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  ADD_REQUEST: "ADD_REQUEST",
  ADD_TIMING_MARKER: "ADD_TIMING_MARKER",
  BATCH_ACTIONS: "BATCH_ACTIONS",
  BATCH_ENABLE: "BATCH_ENABLE",
  CLEAR_REQUESTS: "CLEAR_REQUESTS",
  CLEAR_TIMING_MARKERS: "CLEAR_TIMING_MARKERS",
  CLONE_SELECTED_REQUEST: "CLONE_SELECTED_REQUEST",
  ENABLE_REQUEST_FILTER_TYPE_ONLY: "ENABLE_REQUEST_FILTER_TYPE_ONLY",
  OPEN_NETWORK_DETAILS: "OPEN_NETWORK_DETAILS",
  OPEN_STATISTICS: "OPEN_STATISTICS",
  REMOVE_SELECTED_CUSTOM_REQUEST: "REMOVE_SELECTED_CUSTOM_REQUEST",
  RESET_COLUMNS: "RESET_COLUMNS",
  SELECT_REQUEST: "SELECT_REQUEST",
  SELECT_DETAILS_PANEL_TAB: "SELECT_DETAILS_PANEL_TAB",
  SEND_CUSTOM_REQUEST: "SEND_CUSTOM_REQUEST",
  SET_REQUEST_FILTER_TEXT: "SET_REQUEST_FILTER_TEXT",
  SORT_BY: "SORT_BY",
  TOGGLE_COLUMN: "TOGGLE_COLUMN",
  TOGGLE_REQUEST_FILTER_TYPE: "TOGGLE_REQUEST_FILTER_TYPE",
  UPDATE_REQUEST: "UPDATE_REQUEST",
  WATERFALL_RESIZE: "WATERFALL_RESIZE",
};

// Descriptions for what this frontend is currently doing.
const ACTIVITY_TYPE = {
  // Standing by and handling requests normally.
  NONE: 0,

  // Forcing the target to reload with cache enabled or disabled.
  RELOAD: {
    WITH_CACHE_ENABLED: 1,
    WITH_CACHE_DISABLED: 2,
    WITH_CACHE_DEFAULT: 3
  },

  // Enabling or disabling the cache without triggering a reload.
  ENABLE_CACHE: 3,
  DISABLE_CACHE: 4
};

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the monitored target begins and finishes navigating.
  TARGET_WILL_NAVIGATE: "NetMonitor:TargetWillNavigate",
  TARGET_DID_NAVIGATE: "NetMonitor:TargetNavigate",

  // When a network or timeline event is received.
  // See https://developer.mozilla.org/docs/Tools/Web_Console/remoting for
  // more information about what each packet is supposed to deliver.
  NETWORK_EVENT: "NetMonitor:NetworkEvent",
  TIMELINE_EVENT: "NetMonitor:TimelineEvent",

  // When a network event is added to the view
  REQUEST_ADDED: "NetMonitor:RequestAdded",

  // When request headers begin and finish receiving.
  UPDATING_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdating:RequestHeaders",
  RECEIVED_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdated:RequestHeaders",

  // When request cookies begin and finish receiving.
  UPDATING_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdating:RequestCookies",
  RECEIVED_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdated:RequestCookies",

  // When request post data begins and finishes receiving.
  UPDATING_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdating:RequestPostData",
  RECEIVED_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdated:RequestPostData",

  // When security information begins and finishes receiving.
  UPDATING_SECURITY_INFO: "NetMonitor::NetworkEventUpdating:SecurityInfo",
  RECEIVED_SECURITY_INFO: "NetMonitor::NetworkEventUpdated:SecurityInfo",

  // When response headers begin and finish receiving.
  UPDATING_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdating:ResponseHeaders",
  RECEIVED_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdated:ResponseHeaders",

  // When response cookies begin and finish receiving.
  UPDATING_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdating:ResponseCookies",
  RECEIVED_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdated:ResponseCookies",

  // When event timings begin and finish receiving.
  UPDATING_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdating:EventTimings",
  RECEIVED_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdated:EventTimings",

  // When response content begins, updates and finishes receiving.
  STARTED_RECEIVING_RESPONSE: "NetMonitor:NetworkEventUpdating:ResponseStart",
  UPDATING_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdating:ResponseContent",
  RECEIVED_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdated:ResponseContent",

  // When the request post params are displayed in the UI.
  REQUEST_POST_PARAMS_DISPLAYED: "NetMonitor:RequestPostParamsAvailable",

  // When the image response thumbnail is displayed in the UI.
  RESPONSE_IMAGE_THUMBNAIL_DISPLAYED:
    "NetMonitor:ResponseImageThumbnailAvailable",

  // Fired when charts have been displayed in the PerformanceStatisticsView.
  PLACEHOLDER_CHARTS_DISPLAYED: "NetMonitor:PlaceholderChartsDisplayed",
  PRIMED_CACHE_CHART_DISPLAYED: "NetMonitor:PrimedChartsDisplayed",
  EMPTY_CACHE_CHART_DISPLAYED: "NetMonitor:EmptyChartsDisplayed",

  // Fired once the NetMonitorController establishes a connection to the debug
  // target.
  CONNECTED: "connected",
};

const HEADERS = [
  {
    name: "status",
    label: "status3",
    canFilter: true,
    filterKey: "status-code"
  },
  {
    name: "method",
    canFilter: true,
  },
  {
    name: "file",
    boxName: "icon-and-file",
    canFilter: false,
  },
  {
    name: "protocol",
    canFilter: true,
  },
  {
    name: "domain",
    boxName: "security-and-domain",
    canFilter: true,
  },
  {
    name: "remoteip",
    canFilter: true,
    filterKey: "remote-ip",
  },
  {
    name: "cause",
    canFilter: true,
  },
  {
    name: "type",
    canFilter: false,
  },
  {
    name: "transferred",
    canFilter: true,
  },
  {
    name: "contentSize",
    boxName: "size",
    filterKey: "size",
    canFilter: true,
  },
  {
    name: "waterfall",
    canFilter: false,
  }
];

const general = {
  ACTIVITY_TYPE,
  EVENTS,
  FILTER_SEARCH_DELAY: 200,
  HEADERS,
  // 100 KB in bytes
  SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE: 102400,
};

// flatten constants
module.exports = Object.assign({}, general, actionTypes);
