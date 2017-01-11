/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals window, dumpn, gNetwork, $, EVENTS, NetMonitorView */

"use strict";

const { Task } = require("devtools/shared/task");
const { writeHeaderText,
        getKeyWithEvent,
        getUrlQuery,
        parseQueryString } = require("./request-utils");
const Actions = require("./actions/index");

/**
 * Functions handling the custom request view.
 */
function CustomRequestView() {
  dumpn("CustomRequestView was instantiated");
}

CustomRequestView.prototype = {
  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize: function () {
    dumpn("Initializing the CustomRequestView");

    this.updateCustomRequestEvent = getKeyWithEvent(this.onUpdate.bind(this));
    $("#custom-pane").addEventListener("input",
      this.updateCustomRequestEvent, false);
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy: function () {
    dumpn("Destroying the CustomRequestView");

    $("#custom-pane").removeEventListener("input",
      this.updateCustomRequestEvent, false);
  },

  /**
   * Populates this view with the specified data.
   *
   * @param object data
   *        The data source (this should be the attachment of a request item).
   * @return object
   *        Returns a promise that resolves upon population the view.
   */
  populate: Task.async(function* (data) {
    $("#custom-url-value").value = data.url;
    $("#custom-method-value").value = data.method;
    this.updateCustomQuery(data.url);

    if (data.requestHeaders) {
      let headers = data.requestHeaders.headers;
      $("#custom-headers-value").value = writeHeaderText(headers);
    }
    if (data.requestPostData) {
      let postData = data.requestPostData.postData.text;
      $("#custom-postdata-value").value = yield gNetwork.getString(postData);
    }

    window.emit(EVENTS.CUSTOMREQUESTVIEW_POPULATED);
  }),

  /**
   * Handle user input in the custom request form.
   *
   * @param object field
   *        the field that the user updated.
   */
  onUpdate: function (field) {
    let selectedItem = NetMonitorView.RequestsMenu.selectedItem;
    let store = NetMonitorView.RequestsMenu.store;
    let value;

    switch (field) {
      case "method":
        value = $("#custom-method-value").value.trim();
        store.dispatch(Actions.updateRequest(selectedItem.id, { method: value }));
        break;
      case "url":
        value = $("#custom-url-value").value;
        this.updateCustomQuery(value);
        store.dispatch(Actions.updateRequest(selectedItem.id, { url: value }));
        break;
      case "query":
        let query = $("#custom-query-value").value;
        this.updateCustomUrl(query);
        value = $("#custom-url-value").value;
        store.dispatch(Actions.updateRequest(selectedItem.id, { url: value }));
        break;
      case "body":
        value = $("#custom-postdata-value").value;
        store.dispatch(Actions.updateRequest(selectedItem.id, {
          requestPostData: {
            postData: { text: value }
          }
        }));
        break;
      case "headers":
        let headersText = $("#custom-headers-value").value;
        value = parseHeadersText(headersText);
        store.dispatch(Actions.updateRequest(selectedItem.id, {
          requestHeaders: { headers: value }
        }));
        break;
    }
  },

  /**
   * Update the query string field based on the url.
   *
   * @param object url
   *        The URL to extract query string from.
   */
  updateCustomQuery: function (url) {
    const paramsArray = parseQueryString(getUrlQuery(url));

    if (!paramsArray) {
      $("#custom-query").hidden = true;
      return;
    }

    $("#custom-query").hidden = false;
    $("#custom-query-value").value = writeQueryText(paramsArray);
  },

  /**
   * Update the url based on the query string field.
   *
   * @param object queryText
   *        The contents of the query string field.
   */
  updateCustomUrl: function (queryText) {
    let params = parseQueryText(queryText);
    let queryString = writeQueryString(params);

    let url = $("#custom-url-value").value;
    let oldQuery = getUrlQuery(url);
    let path = url.replace(oldQuery, queryString);

    $("#custom-url-value").value = path;
  }
};

/**
 * Parse text representation of multiple HTTP headers.
 *
 * @param string text
 *        Text of headers
 * @return array
 *         Array of headers info {name, value}
 */
function parseHeadersText(text) {
  return parseRequestText(text, "\\S+?", ":");
}

/**
 * Parse readable text list of a query string.
 *
 * @param string text
 *        Text of query string representation
 * @return array
 *         Array of query params {name, value}
 */
function parseQueryText(text) {
  return parseRequestText(text, ".+?", "=");
}

/**
 * Parse a text representation of a name[divider]value list with
 * the given name regex and divider character.
 *
 * @param string text
 *        Text of list
 * @return array
 *         Array of headers info {name, value}
 */
function parseRequestText(text, namereg, divider) {
  let regex = new RegExp("(" + namereg + ")\\" + divider + "\\s*(.+)");
  let pairs = [];

  for (let line of text.split("\n")) {
    let matches;
    if (matches = regex.exec(line)) { // eslint-disable-line
      let [, name, value] = matches;
      pairs.push({name: name, value: value});
    }
  }
  return pairs;
}

/**
 * Write out a list of query params into a chunk of text
 *
 * @param array params
 *        Array of query params {name, value}
 * @return string
 *         List of query params in text format
 */
function writeQueryText(params) {
  return params.map(({name, value}) => name + "=" + value).join("\n");
}

/**
 * Write out a list of query params into a query string
 *
 * @param array params
 *        Array of query  params {name, value}
 * @return string
 *         Query string that can be appended to a url.
 */
function writeQueryString(params) {
  return params.map(({name, value}) => name + "=" + value).join("&");
}

exports.CustomRequestView = CustomRequestView;
