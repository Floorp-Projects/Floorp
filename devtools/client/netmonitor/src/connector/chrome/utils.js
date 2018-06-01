/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Request, Header, PostData } = require("./request");
const { State, ResponseContent, Timings} = require("./response");
const { getBulkLoader } = require("./bulk-loader");

class Payload {
  constructor() {
    this.payload = {};
    this.update = this.update.bind(this);
  }
  async update(payload) {
    const { request, response, requestId, timestamp,
          content, dataLength, encodedDataLength } = payload;
    const {
      headers,
      postData,
      timing
    } = (request ? request : response) || {};

    const header = await this.mappingHeader(requestId, headers);

    this.requestId = requestId;

    this.updateTimestamp(timestamp);
    const data = await this.mappingAll(
      requestId,
      {
        payload, response, postData,
        header, content, timing,
        dataLength, encodedDataLength
      }
    );
    return data;
  }

  log(reason, info) {
    this.updatePayload({
      type: reason,
      log: info
    });
  }

  updateTimestamp(timestamp) {
    const {request} = this.payload;
    this.updatePayload(
      request ? { response: timestamp } : { request: timestamp }
    );
  }

  updatePayload(data) {
    this.payload = Object.assign({}, this.payload, data);
  }

  async mappingAll(requestId, data) {
    const {payload, response, postData,
         header, content, timing,
         dataLength, encodedDataLength } = data;
    const [requests, headers, post,
         status, timings, responses]
        = await Promise.all(
          [
            this.mappingRequest(requestId, payload),
            header,
            this.mappingRequestPostData(requestId, postData, header),
            this.mappingResponseStatus(requestId, response, header),
            this.mappingTiming(requestId, timing),
            this.mappingResponseContent(requestId, response, content)
          ]);
    this.updatePayload({
      requests, headers, post, status, timings, responses, dataLength, encodedDataLength
    });
    return [ requests, headers, post, status, timings, responses ];
  }

  async mappingTiming(requestId, timing) {
    return !timing ? undefined : Timings(requestId, timing);
  }

  async mappingRequest(requestId, payload) {
    const {request} = payload;
    return !request ? undefined : Request(requestId, payload);
  }

  async mappingHeader(requestId, headers) {
    return !headers ? undefined : Header(requestId, headers);
  }

  async mappingRequestPostData(requestId, postData, headers) {
    return !postData ? undefined : PostData(requestId, postData, headers);
  }

  async mappingResponseStatus(requestId, response, header) {
    return !response ? undefined : State(response, header);
  }

  async mappingResponseContent(requestId, response, content) {
    return !response || !content ?
      undefined : ResponseContent(requestId, response, content);
  }
}
class Payloads {
  constructor() {
    this.payloads = new Map();
  }

  add(id) {
    if (!this.payloads.has(id)) {
      this.payloads.set(id, new Payload());
    }
    return this.payloads.get(id);
  }

  get(id) {
    return this.payloads.has(id) ?
      this.payloads.get(id) : undefined;
  }

  clear() {
    this.payloads.clear();
    const loader = getBulkLoader();
    loader.reset();
  }
}

module.exports = {
  Payload, Payloads
};
