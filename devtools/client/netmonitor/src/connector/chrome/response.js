/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function ResponseInfo(id, response, content) {
  const {
    mimeType
  } = response;
  const {body, base64Encoded} = content;
  return {
    from: id,
    content: {
      mimeType: mimeType,
      text: !body ? "" : body,
      size: !body ? 0 : body.length,
      encoding: base64Encoded ? "base64" : undefined
    }
  };
}

function ResponseContent(id, response, content) {
  const {body} = content;
  const {mimeType, encodedDataLength} = response;
  const responseContent = ResponseInfo(id, response, content);
  const payload = Object.assign(
    {
      responseContent,
      contentSize: !body ? 0 : body.length,
      transferredSize: encodedDataLength, // TODO: verify
      mimeType: mimeType
    }, body);
  return payload;
}

/**
 * Not support on current version.
 * unstable method: Security
 * cause: https://chromedevtools.github.io/devtools-protocol/tot/Security/
 */
function SecurityDetails(id, security) {
  // TODO : verify

  return {};
}

function Timings(id, timing) {
  // TODO : implement
  const {
    dnsStart,
    dnsEnd,
    connectStart,
    connectEnd,
    sendStart,
    sendEnd,
    receiveHeadersEnd
  } = timing;
  const dns = parseInt(dnsEnd - dnsStart, 10);
  const connect = parseInt(connectEnd - connectStart, 10);
  const send = parseInt(sendEnd - sendStart, 10);
  const total = parseInt(receiveHeadersEnd, 10);
  return {
    from: id,
    timings: {
      blocked: 0,
      dns: dns,
      connect: connect,
      send: send,
      wait: parseInt(receiveHeadersEnd - (send + connect + dns), 10),
      receive: 0,
    },
    totalTime: total,
  };
}
function State(response, headers) {
  const { headersSize } = headers;
  const {
    status,
    statusText,
    remoteIPAddress,
    remotePort
  } = response;
  return {
    remoteAddress: remoteIPAddress,
    remotePort,
    status,
    statusText,
    headersSize
  };
}
module.exports = {
  State,
  Timings,
  ResponseContent,
  SecurityDetails
};
