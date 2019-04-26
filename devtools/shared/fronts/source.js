/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { sourceSpec } = require("devtools/shared/specs/source");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

/**
 * A SourceFront provides a way to access the source text of a script.
 *
 * @param client DebuggerClient
 *        The Debugger Client instance.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 * @param activeThread ThreadClient
 *        The thread client parent. Used until the SourceFront marshalls LongStringFront
 *        and ArrayBuffer.
 */
class SourceFront extends FrontClassWithSpec(sourceSpec) {
  constructor(client, form, activeThread) {
    super(client);
    this._url = form.url;
    this._activeThread = activeThread;
    // this is here for the time being, until the source front is managed
    // via protocol.js marshalling
    this.actorID = form.actor;
    this.manage(this);
  }

  get actor() {
    return this.actorID;
  }

  get url() {
    return this._url;
  }

  // Alias for source.blackbox to avoid changing protocol.js packets
  blackBox(range) {
    return this.blackbox(range);
  }

  // Alias for source.unblackbox to avoid changing protocol.js packets
  unblackBox() {
    return this.unblackbox();
  }

  /**
   * Get a long string grip for this SourceFront's source.
   */
  async source() {
    const response = await this.onSource();
    return this._onSourceResponse(response);
  }

  _onSourceResponse(response) {
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
  }
}

exports.SourceFront = SourceFront;
registerFront(SourceFront);
