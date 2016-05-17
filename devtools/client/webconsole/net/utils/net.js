/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const mimeCategoryMap = {
  "text/plain": "txt",
  "application/octet-stream": "bin",
  "text/html": "html",
  "text/xml": "html",
  "application/xml": "html",
  "application/rss+xml": "html",
  "application/atom+xml": "html",
  "application/xhtml+xml": "html",
  "application/mathml+xml": "html",
  "application/rdf+xml": "html",
  "text/css": "css",
  "application/x-javascript": "js",
  "text/javascript": "js",
  "application/javascript": "js",
  "text/ecmascript": "js",
  "application/ecmascript": "js",
  "image/jpeg": "image",
  "image/jpg": "image",
  "image/gif": "image",
  "image/png": "image",
  "image/bmp": "image",
  "application/x-shockwave-flash": "plugin",
  "application/x-silverlight-app": "plugin",
  "video/x-flv": "media",
  "audio/mpeg3": "media",
  "audio/x-mpeg-3": "media",
  "video/mpeg": "media",
  "video/x-mpeg": "media",
  "video/webm": "media",
  "video/mp4": "media",
  "video/ogg": "media",
  "audio/ogg": "media",
  "application/ogg": "media",
  "application/x-ogg": "media",
  "application/x-midi": "media",
  "audio/midi": "media",
  "audio/x-mid": "media",
  "audio/x-midi": "media",
  "music/crescendo": "media",
  "audio/wav": "media",
  "audio/x-wav": "media",
  "application/x-woff": "font",
  "application/font-woff": "font",
  "application/x-font-woff": "font",
  "application/x-ttf": "font",
  "application/x-font-ttf": "font",
  "font/ttf": "font",
  "font/woff": "font",
  "application/x-otf": "font",
  "application/x-font-otf": "font"
};

var NetUtils = {};

NetUtils.isImage = function (contentType) {
  if (!contentType) {
    return false;
  }

  contentType = contentType.split(";")[0];
  contentType = contentType.trim();
  return mimeCategoryMap[contentType] == "image";
};

NetUtils.isHTML = function (contentType) {
  if (!contentType) {
    return false;
  }

  contentType = contentType.split(";")[0];
  contentType = contentType.trim();
  return mimeCategoryMap[contentType] == "html";
};

NetUtils.getHeaderValue = function (headers, name) {
  if (!headers) {
    return null;
  }

  name = name.toLowerCase();
  for (let i = 0; i < headers.length; ++i) {
    let headerName = headers[i].name.toLowerCase();
    if (headerName == name) {
      return headers[i].value;
    }
  }
};

NetUtils.parseXml = function (content) {
  let contentType = content.mimeType.split(";")[0];
  contentType = contentType.trim();

  let parser = new DOMParser();
  let doc = parser.parseFromString(content.text, contentType);
  let root = doc.documentElement;

  // Error handling
  let nsURI = "http://www.mozilla.org/newlayout/xml/parsererror.xml";
  if (root.namespaceURI == nsURI && root.nodeName == "parsererror") {
    return null;
  }

  return doc;
};

NetUtils.isURLEncodedRequest = function (file) {
  let mimeType = "application/x-www-form-urlencoded";

  let postData = file.request.postData;
  if (postData && postData.text) {
    let text = postData.text.toLowerCase();
    if (text.startsWith("content-type: " + mimeType)) {
      return true;
    }
  }

  let value = NetUtils.getHeaderValue(file.request.headers, "content-type");
  return value && value.startsWith(mimeType);
};

NetUtils.isMultiPartRequest = function (file) {
  let mimeType = "multipart/form-data";
  let value = NetUtils.getHeaderValue(file.request.headers, "content-type");
  return value && value.startsWith(mimeType);
};

// Exports from this module
module.exports = NetUtils;
