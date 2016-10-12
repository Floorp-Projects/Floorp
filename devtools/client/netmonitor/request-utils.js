"use strict";

const { Ci } = require("chrome");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const { Task } = require("devtools/shared/task");

/**
 * Helper method to get a wrapped function which can be bound to as
 * an event listener directly and is executed only when data-key is
 * present in event.target.
 *
 * @param function callback
 *          Function to execute execute when data-key
 *          is present in event.target.
 * @param bool onlySpaceOrReturn
 *          Flag to indicate if callback should only be called
            when the space or return button is pressed
 * @return function
 *          Wrapped function with the target data-key as the first argument
 *          and the event as the second argument.
 */
exports.getKeyWithEvent = function (callback, onlySpaceOrReturn) {
  return function (event) {
    let key = event.target.getAttribute("data-key");
    let filterKeyboardEvent = !onlySpaceOrReturn ||
                              event.keyCode === KeyCodes.DOM_VK_SPACE ||
                              event.keyCode === KeyCodes.DOM_VK_RETURN;

    if (key && filterKeyboardEvent) {
      callback.call(null, key);
    }
  };
};

/**
 * Extracts any urlencoded form data sections (e.g. "?foo=bar&baz=42") from a
 * POST request.
 *
 * @param object headers
 *        The "requestHeaders".
 * @param object uploadHeaders
 *        The "requestHeadersFromUploadStream".
 * @param object postData
 *        The "requestPostData".
 * @param object getString
          Callback to retrieve a string from a LongStringGrip.
 * @return array
 *        A promise that is resolved with the extracted form data.
 */
exports.getFormDataSections = Task.async(function* (headers, uploadHeaders, postData,
                                                    getString) {
  let formDataSections = [];

  let { headers: requestHeaders } = headers;
  let { headers: payloadHeaders } = uploadHeaders;
  let allHeaders = [...payloadHeaders, ...requestHeaders];

  let contentTypeHeader = allHeaders.find(e => {
    return e.name.toLowerCase() == "content-type";
  });

  let contentTypeLongString = contentTypeHeader ? contentTypeHeader.value : "";

  let contentType = yield getString(contentTypeLongString);

  if (contentType.includes("x-www-form-urlencoded")) {
    let postDataLongString = postData.postData.text;
    let text = yield getString(postDataLongString);

    for (let section of text.split(/\r\n|\r|\n/)) {
      // Before displaying it, make sure this section of the POST data
      // isn't a line containing upload stream headers.
      if (payloadHeaders.every(header => !section.startsWith(header.name))) {
        formDataSections.push(section);
      }
    }
  }

  return formDataSections;
});

/**
 * Form a data: URI given a mime type, encoding, and some text.
 *
 * @param {String} mimeType the mime type
 * @param {String} encoding the encoding to use; if not set, the
 *        text will be base64-encoded.
 * @param {String} text the text of the URI.
 * @return {String} a data: URI
 */
exports.formDataURI = function (mimeType, encoding, text) {
  if (!encoding) {
    encoding = "base64";
    text = btoa(text);
  }
  return "data:" + mimeType + ";" + encoding + "," + text;
};

/**
 * Write out a list of headers into a chunk of text
 *
 * @param array headers
 *        Array of headers info {name, value}
 * @return string text
 *         List of headers in text format
 */
exports.writeHeaderText = function (headers) {
  return headers.map(({name, value}) => name + ": " + value).join("\n");
};

/**
 * Convert a nsIContentPolicy constant to a display string
 */
const LOAD_CAUSE_STRINGS = {
  [Ci.nsIContentPolicy.TYPE_INVALID]: "invalid",
  [Ci.nsIContentPolicy.TYPE_OTHER]: "other",
  [Ci.nsIContentPolicy.TYPE_SCRIPT]: "script",
  [Ci.nsIContentPolicy.TYPE_IMAGE]: "img",
  [Ci.nsIContentPolicy.TYPE_STYLESHEET]: "stylesheet",
  [Ci.nsIContentPolicy.TYPE_OBJECT]: "object",
  [Ci.nsIContentPolicy.TYPE_DOCUMENT]: "document",
  [Ci.nsIContentPolicy.TYPE_SUBDOCUMENT]: "subdocument",
  [Ci.nsIContentPolicy.TYPE_REFRESH]: "refresh",
  [Ci.nsIContentPolicy.TYPE_XBL]: "xbl",
  [Ci.nsIContentPolicy.TYPE_PING]: "ping",
  [Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST]: "xhr",
  [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "objectSubdoc",
  [Ci.nsIContentPolicy.TYPE_DTD]: "dtd",
  [Ci.nsIContentPolicy.TYPE_FONT]: "font",
  [Ci.nsIContentPolicy.TYPE_MEDIA]: "media",
  [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "websocket",
  [Ci.nsIContentPolicy.TYPE_CSP_REPORT]: "csp",
  [Ci.nsIContentPolicy.TYPE_XSLT]: "xslt",
  [Ci.nsIContentPolicy.TYPE_BEACON]: "beacon",
  [Ci.nsIContentPolicy.TYPE_FETCH]: "fetch",
  [Ci.nsIContentPolicy.TYPE_IMAGESET]: "imageset",
  [Ci.nsIContentPolicy.TYPE_WEB_MANIFEST]: "webManifest"
};

exports.loadCauseString = function (causeType) {
  return LOAD_CAUSE_STRINGS[causeType] || "unknown";
};
