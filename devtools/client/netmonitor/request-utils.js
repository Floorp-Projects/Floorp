"use strict";

const { Task } = require("devtools/shared/task");

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
