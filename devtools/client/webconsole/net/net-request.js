/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

// Reps
const { parseURLParams } = require("devtools/client/shared/components/reps/reps");

// Network
const { cancelEvent, isLeftClick } = require("./utils/events");
const NetInfoBody = React.createFactory(require("./components/net-info-body"));
const DataProvider = require("./data-provider");

// Constants
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * This object represents a network log in the Console panel (and in the
 * Network panel in the future).
 * It's associated with an existing log and so, also with an existing
 * element in the DOM.
 *
 * The object neither render no request for more data by default. It only
 * reqisters a click listener to the associated log entry (a network event)
 * and changes the class attribute of the log entry, so a twisty icon
 * appears to indicates that there are more details displayed if the
 * log entry is expanded.
 *
 * When the user expands the log, data are requested from the backend
 * and rendered directly within the Console iframe.
 */
function NetRequest(log) {
  this.initialize(log);
}

NetRequest.prototype = {
  initialize: function (log) {
    this.client = log.consoleFrame.webConsoleClient;
    this.owner = log.consoleFrame.owner;

    // 'this.file' field is following HAR spec.
    // http://www.softwareishard.com/blog/har-12-spec/
    this.file = log.response;
    this.parentNode = log.node;
    this.file.request.queryString = parseURLParams(this.file.request.url);
    this.hasCookies = false;

    // Map of fetched responses (to avoid unnecessary RDP round trip).
    this.cachedResponses = new Map();

    let doc = this.parentNode.ownerDocument;
    let twisty = doc.createElementNS(XHTML_NS, "a");
    twisty.className = "theme-twisty";
    twisty.href = "#";

    let messageBody = this.parentNode.querySelector(".message-body-wrapper");
    this.parentNode.insertBefore(twisty, messageBody);
    this.parentNode.setAttribute("collapsible", true);

    this.parentNode.classList.add("netRequest");

    // Register a click listener.
    this.addClickListener();
  },

  addClickListener: function () {
    // Add an event listener to toggle the expanded state when clicked.
    // The event bubbling is canceled if the user clicks on the log
    // itself (not on the expanded body), so opening of the default
    // modal dialog is avoided.
    this.parentNode.addEventListener("click", (event) => {
      if (!isLeftClick(event)) {
        return;
      }

      // Clicking on the toggle button or the method expands/collapses
      // the body with HTTP details.
      let classList = event.originalTarget.classList;
      if (!(classList.contains("theme-twisty") ||
        classList.contains("method"))) {
        return;
      }

      // Alright, the user is clicking fine, let's open HTTP details!
      this.onToggleBody(event);

      // Avoid the default modal dialog
      cancelEvent(event);
    }, true);
  },

  onToggleBody: function (event) {
    let target = event.currentTarget;
    let logRow = target.closest(".netRequest");
    logRow.classList.toggle("opened");

    let twisty = this.parentNode.querySelector(".theme-twisty");
    if (logRow.classList.contains("opened")) {
      twisty.setAttribute("open", true);
    } else {
      twisty.removeAttribute("open");
    }

    let isOpen = logRow.classList.contains("opened");
    if (isOpen) {
      this.renderBody();
    } else {
      this.closeBody();
    }
  },

  updateCookies: function(method, response) {
    // TODO: This code will be part of a reducer.
    let result;
    if (response.cookies > 0 &&
        ["requestCookies", "responseCookies"].includes(method)) {
      this.hasCookies = true;
      this.refresh();
    }
  },

  /**
   * Executed when 'networkEventUpdate' is received from the backend.
   */
  updateBody: function (response) {
    // 'networkEventUpdate' event indicates that there are new data
    // available on the backend. The following logic checks the response
    // cache and if this data has been already requested before they
    // need to be updated now (re-requested).
    let method = response.updateType;
    this.updateCookies(method, response);
    if (this.cachedResponses.get(method)) {
      this.cachedResponses.delete(method);
      this.requestData(method);
    }
  },

  /**
   * Close network inline preview body.
   */
  closeBody: function () {
    this.netInfoBodyBox.remove();
  },

  /**
   * Render network inline preview body.
   */
  renderBody: function () {
    let messageBody = this.parentNode.querySelector(".message-body-wrapper");

    // Create box for all markup rendered by ReactJS. Since we are
    // rendering within webconsole.xul (i.e. XUL document) we need
    // to explicitly specify XHTML namespace.
    let doc = messageBody.ownerDocument;
    this.netInfoBodyBox = doc.createElementNS(XHTML_NS, "div");
    this.netInfoBodyBox.classList.add("netInfoBody");
    messageBody.appendChild(this.netInfoBodyBox);

    // As soon as Redux is in place state and actions will come from
    // separate modules.
    let body = NetInfoBody({
      actions: this,
      sourceMapService: this.owner.sourceMapURLService,
    });

    // Render net info body!
    this.body = ReactDOM.render(body, this.netInfoBodyBox);

    this.refresh();
  },

  /**
   * Render top level ReactJS component.
   */
  refresh: function () {
    if (!this.netInfoBodyBox) {
      return;
    }

    // TODO: As soon as Redux is in place there will be reducer
    // computing a new state.
    let newState = Object.assign({}, this.body.state, {
      data: this.file,
      hasCookies: this.hasCookies
    });

    this.body.setState(newState);
  },

  // Communication with the backend

  requestData: function (method) {
    // If the response has already been received bail out.
    let response = this.cachedResponses.get(method);
    if (response) {
      return;
    }

    // Set an attribute indicating that this net log is waiting for
    // data coming from the backend. Intended mainly for tests.
    this.parentNode.setAttribute("loading", "true");

    let actor = this.file.actor;
    DataProvider.requestData(this.client, actor, method).then(args => {
      this.cachedResponses.set(method, args);
      this.onRequestData(method, args);

      if (!DataProvider.hasPendingRequests()) {
        this.parentNode.removeAttribute("loading");

        // Fire an event indicating that all pending requests for
        // data from the backend has finished. Intended for tests.
        // Do it asynchronously so, it's done after all handlers
        // for the current promise are executed.
        setTimeout(() => {
          let event = document.createEvent("Event");
          event.initEvent("netlog-no-pending-requests", true, true);
          this.parentNode.dispatchEvent(event);
        });
      }
    });
  },

  onRequestData: function (method, response) {
    // TODO: This code will be part of a reducer.
    let result;
    switch (method) {
      case "requestHeaders":
        result = this.onRequestHeaders(response);
        break;
      case "responseHeaders":
        result = this.onResponseHeaders(response);
        break;
      case "requestCookies":
        result = this.onRequestCookies(response);
        break;
      case "responseCookies":
        result = this.onResponseCookies(response);
        break;
      case "responseContent":
        result = this.onResponseContent(response);
        break;
      case "requestPostData":
        result = this.onRequestPostData(response);
        break;
    }

    result.then(() => {
      this.refresh();
    });
  },

  onRequestHeaders: function (response) {
    this.file.request.headers = response.headers;

    return this.resolveHeaders(this.file.request.headers);
  },

  onResponseHeaders: function (response) {
    this.file.response.headers = response.headers;

    return this.resolveHeaders(this.file.response.headers);
  },

  onResponseContent: function (response) {
    let content = response.content;

    for (let p in content) {
      this.file.response.content[p] = content[p];
    }

    return Promise.resolve();
  },

  onRequestPostData: function (response) {
    this.file.request.postData = response.postData;
    return Promise.resolve();
  },

  onRequestCookies: function (response) {
    this.file.request.cookies = response.cookies;
    return this.resolveHeaders(this.file.request.cookies);
  },

  onResponseCookies: function (response) {
    this.file.response.cookies = response.cookies;
    return this.resolveHeaders(this.file.response.cookies);
  },

  onViewSourceInDebugger: function (frame) {
    this.owner.viewSourceInDebugger(frame.source, frame.line);
  },

  resolveHeaders: function (headers) {
    let promises = [];

    for (let header of headers) {
      if (typeof header.value == "object") {
        promises.push(this.resolveString(header.value).then(value => {
          header.value = value;
        }));
      }
    }

    return Promise.all(promises);
  },

  resolveString: function (object, propName) {
    let stringGrip = object[propName];
    if (typeof stringGrip == "object") {
      DataProvider.resolveString(this.client, stringGrip).then(args => {
        object[propName] = args;
        this.refresh();
      });
    }
  }
};

// Exports from this module
module.exports = NetRequest;
