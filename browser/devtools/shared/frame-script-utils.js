/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const Cu = Components.utils;

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
devtools.lazyImporter(this, "promise", "resource://gre/modules/Promise.jsm", "Promise");
devtools.lazyImporter(this, "Task", "resource://gre/modules/Task.jsm", "Task");

addMessageListener("devtools:test:history", function ({ data }) {
  content.history[data.direction]();
});

addMessageListener("devtools:test:navigate", function ({ data }) {
  content.location = data.location;
});

addMessageListener("devtools:test:reload", function ({ data }) {
  data = data || {};
  content.location.reload(data.forceget);
});

addMessageListener("devtools:test:console", function ({ data }) {
  let method = data.shift();
  content.console[method].apply(content.console, data);
});

/**
 * Performs a single XMLHttpRequest and returns a promise that resolves once
 * the request has loaded.
 *
 * @param Object data
 *        { method: the request method (default: "GET"),
 *          url: the url to request (default: content.location.href),
 *          body: the request body to send (default: ""),
 *          nocache: append an unique token to the query string (default: true)
 *        }
 *
 * @return Promise A promise that's resolved with object
 *         { status: XMLHttpRequest.status,
 *           response: XMLHttpRequest.response }
 *
 */
function promiseXHR(data) {
  let xhr = new content.XMLHttpRequest();

  let method = data.method || "GET";
  let url = data.url || content.location.href;
  let body = data.body || "";

  if (data.nocache) {
    url += "?devtools-cachebust=" + Math.random();
  }

  let deferred = promise.defer();
  xhr.addEventListener("loadend", function loadend(event) {
    xhr.removeEventListener("loadend", loadend);
    deferred.resolve({ status: xhr.status, response: xhr.response });
  });

  xhr.open(method, url);
  xhr.send(body);
  return deferred.promise;

}

/**
 * Performs XMLHttpRequest request(s) in the context of the page. The data
 * parameter can be either a single object or an array of objects described below.
 * The requests will be performed one at a time in the order they appear in the data.
 *
 * The objects should have following form (any of them can be omitted; defaults
 * shown below):
 * {
 *   method: "GET",
 *   url: content.location.href,
 *   body: "",
 *   nocache: true, // Adds a cache busting random token to the URL
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
addMessageListener("devtools:test:xhr", Task.async(function* ({ data }) {
  let requests = Array.isArray(data) ? data : [data];
  let responses = [];

  for (let request of requests) {
    let response = yield promiseXHR(request);
    responses.push(response);
  }

  sendAsyncMessage("devtools:test:xhr", responses);
}));

// To eval in content, look at `evalInDebuggee` in the head.js of canvasdebugger
// for an example.
addMessageListener("devtools:test:eval", function ({ data }) {
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
  let {selector, propertyName, propertyValue} = msg.data;
  let node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  node.style[propertyName] = propertyValue;

  sendAsyncMessage("devtools:test:setStyle");
});

/**
 * Get information about a DOM element, identified by a selector.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * @return {Object} data Null if selector didn't match any node, otherwise:
 * - {String} tagName.
 * - {String} namespaceURI.
 * - {Number} numChildren The number of children in the element.
 * - {Array} attributes An array of {name, value, namespaceURI} objects.
 * - {String} outerHTML.
 * - {String} innerHTML.
 * - {String} textContent.
 */
addMessageListener("devtools:test:getDomElementInfo", function(msg) {
  let {selector} = msg.data;
  let node = superQuerySelector(selector);

  let info = null;
  if (node) {
    info = {
      tagName: node.tagName,
      namespaceURI: node.namespaceURI,
      numChildren: node.children.length,
      attributes: [...node.attributes].map(({name, value, namespaceURI}) => {
        return {name, value, namespaceURI};
      }),
      outerHTML: node.outerHTML,
      innerHTML: node.innerHTML,
      textContent: node.textContent
    };
  }

  sendAsyncMessage("devtools:test:getDomElementInfo", info);
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
  let {selector, attributeName, attributeValue} = msg.data;
  let node = superQuerySelector(selector);
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
function superQuerySelector(superSelector, root=content.document) {
  let frameIndex = superSelector.indexOf("||");
  if (frameIndex === -1) {
    return root.querySelector(superSelector);
  } else {
    let rootSelector = superSelector.substring(0, frameIndex).trim();
    let childSelector = superSelector.substring(frameIndex+2).trim();
    root = root.querySelector(rootSelector);
    if (!root || !root.contentWindow) {
      return null;
    }

    return superQuerySelector(childSelector, root.contentWindow.document);
  }
}
