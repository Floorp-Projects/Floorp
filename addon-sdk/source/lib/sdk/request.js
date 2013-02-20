/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable"
};

const { ns } = require("./core/namespace");
const { emit } = require("./event/core");
const { merge } = require("./util/object");
const { stringify } = require("./querystring");
const { EventTarget } = require("./event/target");
const { Class } = require("./core/heritage");
const { XMLHttpRequest } = require("./net/xhr");
const apiUtils = require("./deprecated/api-utils");

const response = ns();
const request = ns();

// Instead of creating a new validator for each request, just make one and
// reuse it.
const { validateOptions, validateSingleOption } = new OptionsValidator({
  url: {
    //XXXzpao should probably verify that url is a valid url as well
    is:  ["string"]
  },
  headers: {
    map: function (v) v || {},
    is:  ["object"],
  },
  content: {
    map: function (v) v || null,
    is:  ["string", "object", "null"],
  },
  contentType: {
    map: function (v) v || "application/x-www-form-urlencoded",
    is:  ["string"],
  },
  overrideMimeType: {
    map: function(v) v || null,
    is: ["string", "null"],
  }
});

const REUSE_ERROR = "This request object has been used already. You must " +
                    "create a new one to make a new request."

// Utility function to prep the request since it's the same between GET and
// POST
function runRequest(mode, target) {
  let source = request(target)
  let { xhr, url, content, contentType, headers, overrideMimeType } = source;

  // If this request has already been used, then we can't reuse it.
  // Throw an error.
  if (xhr)
    throw new Error(REUSE_ERROR);

  xhr = source.xhr = new XMLHttpRequest();

  // Build the data to be set. For GET requests, we want to append that to
  // the URL before opening the request.
  let data = stringify(content);
  // If the URL already has ? in it, then we want to just use &
  if (mode == "GET" && data)
    url = url + (/\?/.test(url) ? "&" : "?") + data;

  // open the request
  xhr.open(mode, url);

  // request header must be set after open, but before send
  xhr.setRequestHeader("Content-Type", contentType);

  // set other headers
  Object.keys(headers).forEach(function(name) {
    xhr.setRequestHeader(name, headers[name]);
  });

  // set overrideMimeType
  if (overrideMimeType)
    xhr.overrideMimeType(overrideMimeType);

  // handle the readystate, create the response, and call the callback
  xhr.onreadystatechange = function onreadystatechange() {
    if (xhr.readyState === 4) {
      let response = Response(xhr);
      source.response = response;
      emit(target, 'complete', response);
    }
  };

  // actually send the request.
  // We don't want to send data on GET requests.
  xhr.send(mode !== "GET" ? data : null);
}

const Request = Class({
  extends: EventTarget,
  initialize: function initialize(options) {
    // `EventTarget.initialize` will set event listeners that are named
    // like `onEvent` in this case `onComplete` listener will be set to
    // `complete` event.
    EventTarget.prototype.initialize.call(this, options);

    // Copy normalized options.
    merge(request(this), validateOptions(options));
  },
  get url() { return request(this).url; },
  set url(value) { request(this).url = validateSingleOption('url', value); },
  get headers() { return request(this).headers; },
  set headers(value) {
    return request(this).headers = validateSingleOption('headers', value);
  },
  get content() { return request(this).content; },
  set content(value) {
    request(this).content = validateSingleOption('content', value);
  },
  get contentType() { return request(this).contentType; },
  set contentType(value) {
    request(this).contentType = validateSingleOption('contentType', value);
  },
  get response() { return request(this).response; },
  get: function() {
    runRequest('GET', this);
    return this;
  },
  post: function() {
    runRequest('POST', this);
    return this;
  },
  put: function() {
    runRequest('PUT', this);
    return this;
  }
});
exports.Request = Request;

const Response = Class({
  initialize: function initialize(request) {
    response(this).request = request;
  },
  get text() response(this).request.responseText,
  get xml() {
    throw new Error("Sorry, the 'xml' property is no longer available. " +
                    "see bug 611042 for more information.");
  },
  get status() response(this).request.status,
  get statusText() response(this).request.statusText,
  get json() {
    try {
      return JSON.parse(this.text);
    } catch(error) {
      return null;
    }
  },
  get headers() {
    let headers = {}, lastKey;
    // Since getAllResponseHeaders() will return null if there are no headers,
    // defend against it by defaulting to ""
    let rawHeaders = response(this).request.getAllResponseHeaders() || "";
    rawHeaders.split("\n").forEach(function (h) {
      // According to the HTTP spec, the header string is terminated by an empty
      // line, so we can just skip it.
      if (!h.length) {
        return;
      }

      let index = h.indexOf(":");
      // The spec allows for leading spaces, so instead of assuming a single
      // leading space, just trim the values.
      let key = h.substring(0, index).trim(),
          val = h.substring(index + 1).trim();

      // For empty keys, that means that the header value spanned multiple lines.
      // In that case we should append the value to the value of lastKey with a
      // new line. We'll assume lastKey will be set because there should never
      // be an empty key on the first pass.
      if (key) {
        headers[key] = val;
        lastKey = key;
      }
      else {
        headers[lastKey] += "\n" + val;
      }
    });
    return headers;
  }
});

// apiUtils.validateOptions doesn't give the ability to easily validate single
// options, so this is a wrapper that provides that ability.
function OptionsValidator(rules) {
  return {
    validateOptions: function (options) {
      return apiUtils.validateOptions(options, rules);
    },
    validateSingleOption: function (field, value) {
      // We need to create a single rule object from our listed rules. To avoid
      // JavaScript String warnings, check for the field & default to an empty object.
      let singleRule = {};
      if (field in rules) {
        singleRule[field] = rules[field];
      }
      let singleOption = {};
      singleOption[field] = value;
      // This should throw if it's invalid, which will bubble up & out.
      return apiUtils.validateOptions(singleOption, singleRule)[field];
    }
  };
}
