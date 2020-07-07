/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global addMessageListener, sendAsyncMessage, content */
"use strict";
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const Services = require("Services");

addMessageListener("devtools:test:history", function({ data }) {
  content.history[data.direction]();
});

addMessageListener("devtools:test:navigate", function({ data }) {
  content.location = data.location;
});

addMessageListener("devtools:test:reload", function({ data }) {
  data = data || {};
  content.location.reload(data.forceget);
});

addMessageListener("devtools:test:console", function({ data }) {
  const { method, args, id } = data;
  content.console[method].apply(content.console, args);
  sendAsyncMessage("devtools:test:console:response", { id });
});

/**
 * Performs a single XMLHttpRequest and returns a promise that resolves once
 * the request has loaded.
 *
 * @param Object data
 *        { method: the request method (default: "GET"),
 *          url: the url to request (default: content.location.href),
 *          body: the request body to send (default: ""),
 *          nocache: append an unique token to the query string (default: true),
 *          requestHeaders: set request headers (default: none)
 *        }
 *
 * @return Promise A promise that's resolved with object
 *         { status: XMLHttpRequest.status,
 *           response: XMLHttpRequest.response }
 *
 */
function promiseXHR(data) {
  return new Promise((resolve, reject) => {
    const xhr = new content.XMLHttpRequest();

    const method = data.method || "GET";
    let url = data.url || content.location.href;
    const body = data.body || "";

    if (data.nocache) {
      url += "?devtools-cachebust=" + Math.random();
    }

    xhr.addEventListener(
      "loadend",
      function(event) {
        resolve({ status: xhr.status, response: xhr.response });
      },
      { once: true }
    );

    xhr.open(method, url);

    // Set request headers
    if (data.requestHeaders) {
      data.requestHeaders.forEach(header => {
        xhr.setRequestHeader(header.name, header.value);
      });
    }

    xhr.send(body);
  });
}

/**
 * Performs a single websocket request and returns a promise that resolves once
 * the request has loaded.
 *
 * @param Object data
 *        { url: the url to request (default: content.location.href),
 *          nocache: append an unique token to the query string (default: true),
 *        }
 *
 * @return Promise A promise that's resolved with object
 *         { status: websocket status(101),
 *           response: empty string }
 *
 */
function promiseWS(data) {
  return new Promise((resolve, reject) => {
    let url = data.url;

    if (data.nocache) {
      url += "?devtools-cachebust=" + Math.random();
    }

    /* Create websocket instance */
    const socket = new content.WebSocket(url);

    /* Since we only use HTTP server to mock websocket, so just ignore the error */
    socket.onclose = e => {
      socket.close();
      resolve({
        status: 101,
        response: "",
      });
    };

    socket.onerror = e => {
      socket.close();
      resolve({
        status: 101,
        response: "",
      });
    };
  });
}

/**
 * Performs XMLHttpRequest request(s) in the context of the page. The data
 * parameter can be either a single object or an array of objects described
 * below. The requests will be performed one at a time in the order they appear
 * in the data.
 *
 * The objects should have following form (any of them can be omitted; defaults
 * shown below):
 * {
 *   method: "GET",
 *   url: content.location.href,
 *   body: "",
 *   nocache: true, // Adds a cache busting random token to the URL,
 *   requestHeaders: [{
 *     name: "Content-Type",
 *     value: "application/json"
 *   }]
 * }
 *
 * The handler will respond with devtools:test:xhr message after all requests
 * have finished. Following data will be available for each requests
 * (in the same order as requests):
 * {
 *   status: XMLHttpRequest.status
 *   response: XMLHttpRequest.response
 * }
 */
addMessageListener("devtools:test:xhr", async function({ data }) {
  const requests = Array.isArray(data) ? data : [data];
  const responses = [];

  for (const request of requests) {
    let response = null;
    if (request.ws) {
      response = await promiseWS(request);
    } else {
      response = await promiseXHR(request);
    }
    responses.push(response);
  }

  sendAsyncMessage("devtools:test:xhr", responses);
});

addMessageListener("devtools:test:profiler", function({ data }) {
  const { method, args, id } = data;
  const result = Services.profiler[method](...args);
  sendAsyncMessage("devtools:test:profiler:response", {
    data: result,
    id: id,
  });
});

// To eval in content, look at `evalInDebuggee` in the shared-head.js.
addMessageListener("devtools:test:eval", function({ data }) {
  sendAsyncMessage("devtools:test:eval:response", {
    value: content.eval(data.script),
    id: data.id,
  });
});

addEventListener(
  "load",
  function() {
    sendAsyncMessage("devtools:test:load");
  },
  true
);
