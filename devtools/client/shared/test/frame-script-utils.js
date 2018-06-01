/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global addMessageListener, sendAsyncMessage, content */
"use strict";
const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
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

    xhr.addEventListener("loadend", function(event) {
      resolve({ status: xhr.status, response: xhr.response });
    }, {once: true});

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
    const response = await promiseXHR(request);
    responses.push(response);
  }

  sendAsyncMessage("devtools:test:xhr", responses);
});

addMessageListener("devtools:test:profiler", function({ data }) {
  const { method, args, id } = data;
  const result = Services.profiler[method](...args);
  sendAsyncMessage("devtools:test:profiler:response", {
    data: result,
    id: id
  });
});

// To eval in content, look at `evalInDebuggee` in the shared-head.js.
addMessageListener("devtools:test:eval", function({ data }) {
  sendAsyncMessage("devtools:test:eval:response", {
    value: content.eval(data.script),
    id: data.id
  });
});

addEventListener("load", function() {
  sendAsyncMessage("devtools:test:load");
}, true);

/**
 * Set a given style property value on a node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {String} propertyName The name of the property to set.
 * - {String} propertyValue The value for the property.
 */
addMessageListener("devtools:test:setStyle", function(msg) {
  const {selector, propertyName, propertyValue} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  node.style[propertyName] = propertyValue;

  sendAsyncMessage("devtools:test:setStyle");
});

/**
 * Set a given attribute value on a node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {String} attributeName The name of the attribute to set.
 * - {String} attributeValue The value for the attribute.
 */
addMessageListener("devtools:test:setAttribute", function(msg) {
  const {selector, attributeName, attributeValue} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  node.setAttribute(attributeName, attributeValue);

  sendAsyncMessage("devtools:test:setAttribute");
});

/**
 * Like document.querySelector but can go into iframes too.
 * ".container iframe || .sub-container div" will first try to find the node
 * matched by ".container iframe" in the root document, then try to get the
 * content document inside it, and then try to match ".sub-container div" inside
 * this document.
 * Any selector coming before the || separator *MUST* match a frame node.
 * @param {String} superSelector.
 * @return {DOMNode} The node, or null if not found.
 */
function superQuerySelector(superSelector, root = content.document) {
  const frameIndex = superSelector.indexOf("||");
  if (frameIndex === -1) {
    return root.querySelector(superSelector);
  }
  const rootSelector = superSelector.substring(0, frameIndex).trim();
  const childSelector = superSelector.substring(frameIndex + 2).trim();
  root = root.querySelector(rootSelector);
  if (!root || !root.contentWindow) {
    return null;
  }

  return superQuerySelector(childSelector, root.contentWindow.document);
}
