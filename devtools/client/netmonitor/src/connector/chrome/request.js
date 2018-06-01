/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function mappingCallFrames(callFrames) {
  const stacktrace = [];
  callFrames.forEach(
    (frame) => {
      const {
        functionName,
        scriptId,
        url,
        lineNumber,
        columnNumber
      } = frame;
      const stack = {
        scriptId,
        filename: url,
        lineNumber,
        columnNumber,
        functionName,
      };
      stacktrace.push(stack);
    }
  );
  return stacktrace;
}

function Cause(initiator) {
  const {url, type, stack} = initiator;
  const {callFrames} = stack || {};
  if (!stack || !callFrames.length) {
    return undefined;
  }
  const cause = {
    type: type,
    loadingDocumentUri: url,
    stacktrace: mappingCallFrames(callFrames)
  };
  return cause;
}

function Header(id, headers) {
  const header = [];
  let headersSize = 0;
  Object.keys(headers).map((value) => {
    header.push(
      {
        name: value,
        value: headers[value],
      }
    );
    headersSize += value.length + headers[value].length;
  });

  return {
    from: id,
    headers: header,
    headersSize: headersSize,
    rawHeaders: undefined,
  };
}
function PostData(id, postData, header) {
  const {headers, headersSize} = header;
  const payload = {};
  const requestPostData = {
    from: id, postDataDiscarded: false, postData: {}
  };
  if (postData) {
    requestPostData.postData.text = postData;
    payload.requestPostData = Object.assign({}, requestPostData);
    payload.requestHeadersFromUploadStream = {headers, headersSize};
  }
  return payload;
}

/**
 * Not support on current version.
 * unstable method: Network.getCookies
 * cause: https://chromedevtools.github.io/devtools-protocol/tot/Network/#method-getCookies
 */
function Cookie(id, Network) {
  // TODO: verify
}

function Request(id, requestData) {
  const {request, initiator, timestamp} = requestData;
  const {url, method} = request;
  const cause = !initiator ? undefined : Cause(initiator);
  return {
    method, url, cause,
    isXHR: false, // TODO: verify
    startedDateTime: timestamp,
    fromCache: undefined,
    fromServiceWorker: undefined
  };
}

module.exports = {
  Cause,
  Cookie,
  Header,
  Request,
  PostData
};
