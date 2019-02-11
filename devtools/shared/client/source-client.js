/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");

const noop = () => {};

/**
 * A SourceClient provides a way to access the source text of a script.
 *
 * @param client ThreadClient
 *        The thread client parent.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
function SourceClient(client, form) {
  this._form = form;
  this._activeThread = client;
  this._client = client.client;
}

SourceClient.prototype = {
  get _transport() {
    return this._client._transport;
  },
  get actor() {
    return this._form.actor;
  },
  get request() {
    return this._client.request;
  },
  get url() {
    return this._form.url;
  },

  /**
   * Black box this SourceClient's source.
   */
  blackBox: DebuggerClient.requester(
    {
      type: "blackbox",
      range: arg(0),
    },
    {
      telemetry: "BLACKBOX",
    },
  ),

  /**
   * Un-black box this SourceClient's source.
   */
  unblackBox: DebuggerClient.requester(
    {
      type: "unblackbox",
      range: arg(0),
    },
    {
      telemetry: "UNBLACKBOX",
    },
  ),

  /**
   * Get Executable Lines from a source
   */
  getExecutableLines: function(cb = noop) {
    const packet = {
      to: this._form.actor,
      type: "getExecutableLines",
    };

    return this._client.request(packet).then(res => {
      cb(res.lines);
      return res.lines;
    });
  },

  getBreakpointPositions: function(query) {
    const packet = {
      to: this._form.actor,
      type: "getBreakpointPositions",
      query,
    };
    return this._client.request(packet);
  },

  getBreakpointPositionsCompressed: function(query) {
    const packet = {
      to: this._form.actor,
      type: "getBreakpointPositionsCompressed",
      query,
    };
    return this._client.request(packet);
  },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function() {
    const packet = {
      to: this._form.actor,
      type: "source",
    };
    return this._client.request(packet).then(response => {
      return this._onSourceResponse(response);
    });
  },

  _onSourceResponse: function(response) {
    if (typeof response.source === "string") {
      return response;
    }

    const { contentType, source } = response;
    if (source.type === "arrayBuffer") {
      const arrayBuffer = this._activeThread.threadArrayBuffer(source);
      return arrayBuffer.slice(0, arrayBuffer.length).then(function(resp) {
        if (resp.error) {
          return resp;
        }
        // Keeping str as a string, ArrayBuffer/Uint8Array will not survive
        // setIn/mergeIn operations.
        const str = atob(resp.encoded);
        const newResponse = {
          source: {
            binary: str,
            toString: () => "[wasm]",
          },
          contentType,
        };
        return newResponse;
      });
    }

    const longString = this._activeThread.threadLongString(source);
    return longString.substring(0, longString.length).then(function(resp) {
      if (resp.error) {
        return resp;
      }

      const newResponse = {
        source: resp.substring,
        contentType: contentType,
      };
      return newResponse;
    });
  },

  setPausePoints: function(pausePoints) {
    const packet = {
      to: this._form.actor,
      type: "setPausePoints",
      pausePoints,
    };
    return this._client.request(packet);
  },
};

module.exports = SourceClient;
