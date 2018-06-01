/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {DebuggerClient} = require("devtools/shared/client/debugger-client");
loader.lazyRequireGetter(this, "BreakpointClient", "devtools/shared/client/breakpoint-client");

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
  this._isBlackBoxed = form.isBlackBoxed;
  this._isPrettyPrinted = form.isPrettyPrinted;
  this._activeThread = client;
  this._client = client.client;
}

SourceClient.prototype = {
  get _transport() {
    return this._client._transport;
  },
  get isBlackBoxed() {
    return this._isBlackBoxed;
  },
  get isPrettyPrinted() {
    return this._isPrettyPrinted;
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
   *
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  blackBox: DebuggerClient.requester({
    type: "blackbox"
  }, {
    after: function(response) {
      if (!response.error) {
        this._isBlackBoxed = true;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return response;
    }
  }),

  /**
   * Un-black box this SourceClient's source.
   *
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  unblackBox: DebuggerClient.requester({
    type: "unblackbox"
  }, {
    after: function(response) {
      if (!response.error) {
        this._isBlackBoxed = false;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return response;
    }
  }),

  /**
   * Get Executable Lines from a source
   *
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  getExecutableLines: function(cb = noop) {
    const packet = {
      to: this._form.actor,
      type: "getExecutableLines"
    };

    return this._client.request(packet).then(res => {
      cb(res.lines);
      return res.lines;
    });
  },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function(callback = noop) {
    const packet = {
      to: this._form.actor,
      type: "source"
    };
    return this._client.request(packet).then(response => {
      return this._onSourceResponse(response, callback);
    });
  },

  /**
   * Pretty print this source's text.
   */
  prettyPrint: function(indent, callback = noop) {
    const packet = {
      to: this._form.actor,
      type: "prettyPrint",
      indent
    };
    return this._client.request(packet).then(response => {
      if (!response.error) {
        this._isPrettyPrinted = true;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      return this._onSourceResponse(response, callback);
    });
  },

  /**
   * Stop pretty printing this source's text.
   */
  disablePrettyPrint: function(callback = noop) {
    const packet = {
      to: this._form.actor,
      type: "disablePrettyPrint"
    };
    return this._client.request(packet).then(response => {
      if (!response.error) {
        this._isPrettyPrinted = false;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      return this._onSourceResponse(response, callback);
    });
  },

  _onSourceResponse: function(response, callback) {
    if (response.error) {
      callback(response);
      return response;
    }

    if (typeof response.source === "string") {
      callback(response);
      return response;
    }

    const { contentType, source } = response;
    if (source.type === "arrayBuffer") {
      const arrayBuffer = this._activeThread.threadArrayBuffer(source);
      return arrayBuffer.slice(0, arrayBuffer.length).then(function(resp) {
        if (resp.error) {
          callback(resp);
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
        callback(newResponse);
        return newResponse;
      });
    }

    const longString = this._activeThread.threadLongString(source);
    return longString.substring(0, longString.length).then(function(resp) {
      if (resp.error) {
        callback(resp);
        return resp;
      }

      const newResponse = {
        source: resp.substring,
        contentType: contentType
      };
      callback(newResponse);
      return newResponse;
    });
  },

  /**
   * Request to set a breakpoint in the specified location.
   *
   * @param object location
   *        The location and condition of the breakpoint in
   *        the form of { line[, column, condition] }.
   * @param function onResponse
   *        Called with the thread's response.
   */
  setBreakpoint: function({ line, column, condition, noSliding }, onResponse = noop) {
    // A helper function that sets the breakpoint.
    const doSetBreakpoint = callback => {
      const root = this._client.mainRoot;
      const location = {
        line,
        column,
      };

      const packet = {
        to: this.actor,
        type: "setBreakpoint",
        location,
        condition,
        noSliding,
      };

      // Backwards compatibility: send the breakpoint request to the
      // thread if the server doesn't support Debugger.Source actors.
      if (!root.traits.debuggerSourceActors) {
        packet.to = this._activeThread.actor;
        packet.location.url = this.url;
      }

      return this._client.request(packet).then(response => {
        // Ignoring errors, since the user may be setting a breakpoint in a
        // dead script that will reappear on a page reload.
        let bpClient;
        if (response.actor) {
          bpClient = new BreakpointClient(
            this._client,
            this,
            response.actor,
            location,
            root.traits.conditionalBreakpoints ? condition : undefined
          );
        }
        onResponse(response, bpClient);
        if (callback) {
          callback();
        }
        return [response, bpClient];
      });
    };

    // If the debuggee is paused, just set the breakpoint.
    if (this._activeThread.paused) {
      return doSetBreakpoint();
    }
    // Otherwise, force a pause in order to set the breakpoint.
    return this._activeThread.interrupt().then(response => {
      if (response.error) {
        // Can't set the breakpoint if pausing failed.
        onResponse(response);
        return response;
      }

      const { type, why } = response;
      const cleanUp = type == "paused" && why.type == "interrupted"
            ? () => this._activeThread.resume()
            : noop;

      return doSetBreakpoint(cleanUp);
    });
  },

  setPausePoints: function(pausePoints) {
    const packet = {
      to: this._form.actor,
      type: "setPausePoints",
      pausePoints
    };
    return this._client.request(packet);
  },
};

module.exports = SourceClient;
