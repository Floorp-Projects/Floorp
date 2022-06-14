/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  ADD_REQUEST: "ADD_REQUEST",
  SET_EVENT_STREAM_FLAG: "SET_EVENT_STREAM_FLAG",
  ADD_TIMING_MARKER: "ADD_TIMING_MARKER",
  ADD_BLOCKED_URL: "ADD_BLOCKED_URL",
  BATCH_ACTIONS: "BATCH_ACTIONS",
  BATCH_ENABLE: "BATCH_ENABLE",
  BATCH_FLUSH: "BATCH_FLUSH",
  BLOCK_SELECTED_REQUEST_DONE: "BLOCK_SELECTED_REQUEST_DONE",
  CLEAR_REQUESTS: "CLEAR_REQUESTS",
  CLEAR_TIMING_MARKERS: "CLEAR_TIMING_MARKERS",
  CLONE_REQUEST: "CLONE_REQUEST",
  CLONE_SELECTED_REQUEST: "CLONE_SELECTED_REQUEST",
  ENABLE_REQUEST_FILTER_TYPE_ONLY: "ENABLE_REQUEST_FILTER_TYPE_ONLY",
  OPEN_NETWORK_DETAILS: "OPEN_NETWORK_DETAILS",
  OPEN_ACTION_BAR: "OPEN_ACTION_BAR",
  RESIZE_NETWORK_DETAILS: "RESIZE_NETWORK_DETAILS",
  RIGHT_CLICK_REQUEST: "RIGHT_CLICK_REQUEST",
  ENABLE_PERSISTENT_LOGS: "ENABLE_PERSISTENT_LOGS",
  DISABLE_BROWSER_CACHE: "DISABLE_BROWSER_CACHE",
  OPEN_STATISTICS: "OPEN_STATISTICS",
  PERSIST_CHANGED: "PERSIST_CHANGED",
  PRESELECT_REQUEST: "PRESELECT_REQUEST",
  REMOVE_SELECTED_CUSTOM_REQUEST: "REMOVE_SELECTED_CUSTOM_REQUEST",
  RESET_COLUMNS: "RESET_COLUMNS",
  SELECT_REQUEST: "SELECT_REQUEST",
  SELECT_DETAILS_PANEL_TAB: "SELECT_DETAILS_PANEL_TAB",
  SELECT_ACTION_BAR_TAB: "SELECT_ACTION_BAR_TAB",
  SEND_CUSTOM_REQUEST: "SEND_CUSTOM_REQUEST",
  SET_REQUEST_FILTER_TEXT: "SET_REQUEST_FILTER_TEXT",
  SORT_BY: "SORT_BY",
  SYNCED_BLOCKED_URLS: "SYNCED_BLOCKED_URLS",
  TOGGLE_BLOCKING_ENABLED: "TOGGLE_BLOCKING_ENABLED",
  REMOVE_BLOCKED_URL: "REMOVE_BLOCKED_URL",
  REMOVE_ALL_BLOCKED_URLS: "REMOVE_ALL_BLOCKED_URLS",
  ENABLE_ALL_BLOCKED_URLS: "ENABLE_ALL_BLOCKED_URLS",
  DISABLE_ALL_BLOCKED_URLS: "DISABLE_ALL_BLOCKED_URLS",
  TOGGLE_BLOCKED_URL: "TOGGLE_BLOCKED_URL",
  UPDATE_BLOCKED_URL: "UPDATE_BLOCKED_URL",
  DISABLE_MATCHING_URLS: "DISABLE_MATCHING_URLS",
  REQUEST_BLOCKING_UPDATE_COMPLETE: "REQUEST_BLOCKING_UPDATE_COMPLETE",
  TOGGLE_COLUMN: "TOGGLE_COLUMN",
  SET_RECORDING_STATE: "SET_RECORDING_STATE",
  TOGGLE_REQUEST_FILTER_TYPE: "TOGGLE_REQUEST_FILTER_TYPE",
  UNBLOCK_SELECTED_REQUEST_DONE: "UNBLOCK_SELECTED_REQUEST_DONE",
  UPDATE_REQUEST: "UPDATE_REQUEST",
  WATERFALL_RESIZE: "WATERFALL_RESIZE",
  SET_COLUMNS_WIDTH: "SET_COLUMNS_WIDTH",
  MSG_ADD: "MSG_ADD",
  MSG_SELECT: "MSG_SELECT",
  MSG_OPEN_DETAILS: "MSG_OPEN_DETAILS",
  MSG_CLEAR: "MSG_CLEAR",
  MSG_TOGGLE_FILTER_TYPE: "MSG_TOGGLE_FILTER_TYPE",
  MSG_TOGGLE_CONTROL: "MSG_TOGGLE_CONTROL",
  MSG_SET_FILTER_TEXT: "MSG_SET_FILTER_TEXT",
  MSG_TOGGLE_COLUMN: "MSG_TOGGLE_COLUMN",
  MSG_RESET_COLUMNS: "MSG_RESET_COLUMNS",
  MSG_CLOSE_CONNECTION: "MSG_CLOSE_CONNECTION",
  SET_HEADERS_URL_PREVIEW_EXPANDED: "SET_HEADERS_URL_PREVIEW_EXPANDED",

  // Search
  ADD_SEARCH_QUERY: "ADD_SEARCH_QUERY",
  ADD_SEARCH_RESULT: "ADD_SEARCH_RESULT",
  CLEAR_SEARCH_RESULTS: "CLEAR_SEARCH_RESULTS",
  ADD_ONGOING_SEARCH: "ADD_ONGOING_SEARCH",
  TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH: "TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH",
  UPDATE_SEARCH_STATUS: "UPDATE_SEARCH_STATUS",
  SET_TARGET_SEARCH_RESULT: "SET_TARGET_SEARCH_RESULT",
};

// Search status types
const SEARCH_STATUS = {
  INITIAL: "INITIAL",
  FETCHING: "FETCHING",
  CANCELED: "CANCELED",
  DONE: "DONE",
  ERROR: "ERROR",
};

const CHANNEL_TYPE = {
  WEB_SOCKET: "WEB_SOCKET",
  EVENT_STREAM: "EVENT_STREAM",
};

// Descriptions for what this frontend is currently doing.
const ACTIVITY_TYPE = {
  // Standing by and handling requests normally.
  NONE: 0,

  // Forcing the target to reload with cache enabled or disabled.
  RELOAD: {
    WITH_CACHE_ENABLED: 1,
    WITH_CACHE_DISABLED: 2,
    WITH_CACHE_DEFAULT: 3,
  },

  // Enabling or disabling the cache without triggering a reload.
  ENABLE_CACHE: 3,
  DISABLE_CACHE: 4,
};

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When a network event is added to the view
  REQUEST_ADDED: "NetMonitor:RequestAdded",

  // When request headers begin receiving.
  UPDATING_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdating:RequestHeaders",

  // When request cookies begin receiving.
  UPDATING_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdating:RequestCookies",

  // When request post data begins receiving.
  UPDATING_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdating:RequestPostData",

  // When security information begins receiving.
  UPDATING_SECURITY_INFO: "NetMonitor:NetworkEventUpdating:SecurityInfo",

  // When response headers begin receiving.
  UPDATING_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdating:ResponseHeaders",

  // When response cookies begin receiving.
  UPDATING_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdating:ResponseCookies",

  // When event timings begin and finish receiving.
  UPDATING_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdating:EventTimings",
  RECEIVED_EVENT_TIMINGS: "NetMonitor:NetworkEventUpdated:EventTimings",

  // When response content updates receiving.
  UPDATING_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdating:ResponseContent",

  UPDATING_RESPONSE_CACHE: "NetMonitor:NetworkEventUpdating:ResponseCache",

  // Fired once the connection is established
  CONNECTED: "connected",

  // When request payload (HTTP details data) are fetched from the backend.
  PAYLOAD_READY: "NetMonitor:PayloadReady",
};

const TEST_EVENTS = {
  // When a network or timeline event is received.
  // See https://firefox-source-docs.mozilla.org/devtools-user/web_console/remoting/ for
  // more information about what each packet is supposed to deliver.
  NETWORK_EVENT: "NetMonitor:NetworkEvent",
  NETWORK_EVENT_UPDATED: "NetMonitor:NetworkEventUpdated",
  TIMELINE_EVENT: "NetMonitor:TimelineEvent",

  // When response content begins receiving.
  STARTED_RECEIVING_RESPONSE: "NetMonitor:NetworkEventUpdating:ResponseStart",

  // When request headers finish receiving.
  RECEIVED_REQUEST_HEADERS: "NetMonitor:NetworkEventUpdated:RequestHeaders",

  // When response headers finish receiving.
  RECEIVED_RESPONSE_HEADERS: "NetMonitor:NetworkEventUpdated:ResponseHeaders",

  // When request cookies finish receiving.
  RECEIVED_REQUEST_COOKIES: "NetMonitor:NetworkEventUpdated:RequestCookies",

  // When request post data finishes receiving.
  RECEIVED_REQUEST_POST_DATA: "NetMonitor:NetworkEventUpdated:RequestPostData",

  // When security information finishes receiving.
  RECEIVED_SECURITY_INFO: "NetMonitor:NetworkEventUpdated:SecurityInfo",

  // When response cookies finish receiving.
  RECEIVED_RESPONSE_COOKIES: "NetMonitor:NetworkEventUpdated:ResponseCookies",

  RECEIVED_RESPONSE_CACHE: "NetMonitor:NetworkEventUpdated:ResponseCache",

  // When response content finishes receiving.
  RECEIVED_RESPONSE_CONTENT: "NetMonitor:NetworkEventUpdated:ResponseContent",

  // When stack-trace finishes receiving.
  RECEIVED_EVENT_STACKTRACE: "NetMonitor:NetworkEventUpdated:StackTrace",

  // When throttling is set on the backend.
  THROTTLING_CHANGED: "NetMonitor:ThrottlingChanged",

  // When a long string is resolved
  LONGSTRING_RESOLVED: "NetMonitor:LongStringResolved",
};

const UPDATE_PROPS = [
  "method",
  "url",
  "remotePort",
  "remoteAddress",
  "status",
  "statusText",
  "httpVersion",
  "isRacing",
  "securityState",
  "securityInfo",
  "securityInfoAvailable",
  "mimeType",
  "contentSize",
  "transferredSize",
  "totalTime",
  "eventTimings",
  "eventTimingsAvailable",
  "headersSize",
  "customQueryValue",
  "requestHeaders",
  "requestHeadersAvailable",
  "requestHeadersFromUploadStream",
  "requestCookies",
  "requestCookiesAvailable",
  "requestPostData",
  "requestPostDataAvailable",
  "responseHeaders",
  "responseHeadersAvailable",
  "responseCookies",
  "responseCookiesAvailable",
  "responseContent",
  "responseContentAvailable",
  "responseCache",
  "responseCacheAvailable",
  "formDataSections",
  "stacktrace",
  "isThirdPartyTrackingResource",
  "referrerPolicy",
  "priority",
  "blockedReason",
  "blockingExtension",
  "channelId",
  "waitingTime",
];

const PANELS = {
  COOKIES: "cookies",
  HEADERS: "headers",
  MESSAGES: "messages",
  REQUEST: "request",
  RESPONSE: "response",
  CACHE: "cache",
  SECURITY: "security",
  STACK_TRACE: "stack-trace",
  TIMINGS: "timings",
  HTTP_CUSTOM_REQUEST: "network-action-bar-HTTP-custom-request",
  SEARCH: "network-action-bar-search",
  BLOCKING: "network-action-bar-blocked",
};

const RESPONSE_HEADERS = [
  "Cache-Control",
  "Connection",
  "Content-Encoding",
  "Content-Length",
  "ETag",
  "Keep-Alive",
  "Last-Modified",
  "Server",
  "Vary",
];

const HEADERS = [
  {
    name: "status",
    label: "status3",
    canFilter: true,
    filterKey: "status-code",
  },
  {
    name: "method",
    canFilter: true,
  },
  {
    name: "domain",
    canFilter: true,
  },
  {
    name: "file",
    canFilter: false,
  },
  {
    name: "url",
    canFilter: true,
  },
  {
    name: "protocol",
    canFilter: true,
  },
  {
    name: "scheme",
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
    name: "initiator",
    canFilter: true,
  },
  {
    name: "type",
    canFilter: false,
  },
  {
    name: "cookies",
    canFilter: false,
  },
  {
    name: "setCookies",
    boxName: "set-cookies",
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
    name: "priority",
    boxName: "priority",
    canFilter: true,
  },
  {
    name: "startTime",
    boxName: "start-time",
    canFilter: false,
    subMenu: "timings",
  },
  {
    name: "endTime",
    boxName: "end-time",
    canFilter: false,
    subMenu: "timings",
  },
  {
    name: "responseTime",
    boxName: "response-time",
    canFilter: false,
    subMenu: "timings",
  },
  {
    name: "duration",
    canFilter: false,
    subMenu: "timings",
  },
  {
    name: "latency",
    canFilter: false,
    subMenu: "timings",
  },
  ...RESPONSE_HEADERS.map(header => ({
    name: header,
    boxName: "response-header",
    canFilter: false,
    subMenu: "responseHeaders",
    noLocalization: true,
  })),
  {
    name: "waterfall",
    canFilter: false,
  },
];

const HEADER_FILTERS = HEADERS.filter(h => h.canFilter).map(
  h => h.filterKey || h.name
);

const FILTER_FLAGS = [
  ...HEADER_FILTERS,
  "set-cookie-domain",
  "set-cookie-name",
  "set-cookie-value",
  "mime-type",
  "larger-than",
  "transferred-larger-than",
  "is",
  "has-response-header",
  "regexp",
];

const FILTER_TAGS = [
  "html",
  "css",
  "js",
  "xhr",
  "fonts",
  "images",
  "media",
  "ws",
  "other",
];

const MESSAGE_HEADERS = [
  {
    name: "data",
    width: "40%",
  },
  {
    name: "size",
    width: "12%",
  },
  {
    name: "opCode",
    width: "9%",
  },
  {
    name: "maskBit",
    width: "9%",
  },
  {
    name: "finBit",
    width: "9%",
  },
  {
    name: "time",
    width: "20%",
  },
  {
    name: "eventName",
    width: "9%",
  },
  {
    name: "lastEventId",
    width: "9%",
  },
  {
    name: "retry",
    width: "9%",
  },
];

const REQUESTS_WATERFALL = {
  BACKGROUND_TICKS_MULTIPLE: 5, // ms
  BACKGROUND_TICKS_SCALES: 3,
  BACKGROUND_TICKS_SPACING_MIN: 10, // px
  BACKGROUND_TICKS_COLOR_RGB: [128, 136, 144],
  // 8-bit value of the alpha component of the tick color
  BACKGROUND_TICKS_OPACITY_MIN: 32,
  BACKGROUND_TICKS_OPACITY_ADD: 32,
  // Colors for timing markers (theme colors, see variables.css)
  DOMCONTENTLOADED_TICKS_COLOR: "highlight-blue",
  LOAD_TICKS_COLOR: "highlight-red",
  // Opacity for the timing markers
  TICKS_COLOR_OPACITY: 192,
  HEADER_TICKS_MULTIPLE: 5, // ms
  HEADER_TICKS_SPACING_MIN: 60, // px
  // Reserve extra space for rendering waterfall time label
  LABEL_WIDTH: 50, // px
};

const TIMING_KEYS = [
  "blocked",
  "dns",
  "connect",
  "ssl",
  "send",
  "wait",
  "receive",
];

// Minimal width of Network Monitor column is 30px, for Waterfall 150px
// Default width of columns (which are not defined in DEFAULT_COLUMNS_DATA) is 8%
const MIN_COLUMN_WIDTH = 30; // in px
const DEFAULT_COLUMN_WIDTH = 8; // in %
/**
 * A mapping of HTTP status codes.
 */
const SUPPORTED_HTTP_CODES = [
  "100",
  "101",
  "200",
  "201",
  "202",
  "203",
  "204",
  "205",
  "206",
  "300",
  "301",
  "302",
  "303",
  "304",
  "307",
  "308",
  "400",
  "401",
  "403",
  "404",
  "405",
  "406",
  "407",
  "408",
  "409",
  "410",
  "411",
  "412",
  "413",
  "414",
  "415",
  "416",
  "417",
  "418",
  "422",
  "425",
  "426",
  "428",
  "429",
  "431",
  "451",
  "500",
  "501",
  "502",
  "503",
  "504",
  "505",
  "511",
];

// Keys are the codes provided by server, values are localization messages
// prefixed by "netmonitor.blocked."
const BLOCKED_REASON_MESSAGES = {
  devtools: "Blocked by DevTools",
  1001: "CORS disabled",
  1002: "CORS Failed",
  1003: "CORS Not HTTP",
  1004: "CORS Multiple Origin Not Allowed",
  1005: "CORS Missing Allow Origin",
  1006: "CORS No Allow Credentials",
  1007: "CORS Allow Origin Not Matching Origin",
  1008: "CORS Missing Allow Credentials",
  1009: "CORS Origin Header Missing",
  1010: "CORS External Redirect Not Allowed",
  1011: "CORS Preflight Did Not Succeed",
  1012: "CORS Invalid Allow Method",
  1013: "CORS Method Not Found",
  1014: "CORS Invalid Allow Header",
  1015: "CORS Missing Allow Header",
  2001: "Malware",
  2002: "Phishing",
  2003: "Unwanted",
  2004: "Tracking",
  2005: "Blocked",
  2006: "Harmful",
  2007: "Cryptomining",
  2008: "Fingerprinting",
  2009: "Socialtracking",
  3001: "Mixed Block",
  4000: "CSP",
  4001: "CSP No Data Protocol",
  4002: "CSP Web Extension",
  4003: "CSP Content Blocked",
  4004: "CSP Data Document",
  4005: "CSP Web Browser",
  4006: "CSP Preload",
  5000: "Not same-origin",
  6000: "Blocked By Extension",
};

const general = {
  ACTIVITY_TYPE,
  EVENTS,
  TEST_EVENTS,
  FILTER_SEARCH_DELAY: 200,
  UPDATE_PROPS,
  HEADERS,
  MESSAGE_HEADERS,
  RESPONSE_HEADERS,
  FILTER_FLAGS,
  FILTER_TAGS,
  REQUESTS_WATERFALL,
  PANELS,
  TIMING_KEYS,
  MIN_COLUMN_WIDTH,
  DEFAULT_COLUMN_WIDTH,
  SUPPORTED_HTTP_CODES,
  BLOCKED_REASON_MESSAGES,
  SEARCH_STATUS,
  AUTO_EXPAND_MAX_LEVEL: 7,
  AUTO_EXPAND_MAX_NODES: 50,
  CHANNEL_TYPE,
};

// flatten constants
module.exports = Object.assign({}, general, actionTypes);
