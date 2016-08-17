/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {
  isImage,
  isHTML,
  getHeaderValue,
  isURLEncodedRequest,
  isMultiPartRequest
} = require("devtools/client/webconsole/net/utils/net");

// Test data
const imageMimeTypes = ["image/jpeg", "image/jpg", "image/gif",
  "image/png", "image/bmp"];

const htmlMimeTypes = ["text/html", "text/xml", "application/xml",
  "application/rss+xml", "application/atom+xml", "application/xhtml+xml",
  "application/mathml+xml", "application/rdf+xml"];

const headers = [{name: "headerName", value: "value1"}];

const har1 = {
  request: {
    postData: {
      text: "content-type: application/x-www-form-urlencoded"
    }
  }
};

const har2 = {
  request: {
    headers: [{
      name: "content-type",
      value: "application/x-www-form-urlencoded"
    }]
  }
};

const har3 = {
  request: {
    headers: [{
      name: "content-type",
      value: "multipart/form-data"
    }]
  }
};

/**
 * Testing API provided by webconsole/net/utils/net.js
 */
function run_test() {
  // isImage
  imageMimeTypes.forEach(mimeType => {
    ok(isImage(mimeType));
  });

  // isHTML
  htmlMimeTypes.forEach(mimeType => {
    ok(isHTML(mimeType));
  });

  // getHeaderValue
  equal(getHeaderValue(headers, "headerName"), "value1");

  // isURLEncodedRequest
  ok(isURLEncodedRequest(har1));
  ok(isURLEncodedRequest(har2));

  // isMultiPartRequest
  ok(isMultiPartRequest(har3));
}
