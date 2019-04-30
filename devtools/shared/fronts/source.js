/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { sourceSpec } = require("devtools/shared/specs/source");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");
const { ArrayBufferFront } = require("devtools/shared/fronts/array-buffer");

/**
 * A SourceFront provides a way to access the source text of a script.
 *
 * @param client DebuggerClient
 *        The Debugger Client instance.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
class SourceFront extends FrontClassWithSpec(sourceSpec) {
  constructor(client, form) {
    super(client);
    this._url = form.url;
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
   * Get a Front for either an ArrayBuffer or LongString
   * for this SourceFront's source.
   */
  async source() {
    const response = await this.onSource();
    return this._onSourceResponse(response);
  }

  _onSourceResponse(response) {
    const { contentType, source } = response;
    if (source instanceof ArrayBufferFront) {
      return source.slice(0, source.length).then(function(resp) {
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

    return source.substring(0, source.length).then(function(resp) {
      if (resp.error) {
        return resp;
      }

      const newResponse = {
        source: resp,
        contentType: contentType,
      };
      return newResponse;
    });
  }
}

exports.SourceFront = SourceFront;
registerFront(SourceFront);
