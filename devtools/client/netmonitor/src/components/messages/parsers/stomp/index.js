/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const frameModule = require("resource://devtools/client/netmonitor/src/components/messages/parsers/stomp/frame.js");
const {
  Parser,
} = require("resource://devtools/client/netmonitor/src/components/messages/parsers/stomp/parser.js");
const {
  parseJSON,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

const { Frame } = frameModule;

function parseStompJs(message) {
  let output;

  function onFrame(rawFrame) {
    const frame = Frame.fromRawFrame(rawFrame);
    const { error, json } = parseJSON(frame.body);

    output = {
      command: frame.command,
      headers: frame.headers,
      body: error ? frame.body : json,
    };
  }
  const onIncomingPing = () => {};
  const parser = new Parser(onFrame, onIncomingPing);

  parser.parseChunk(message);

  return output;
}

module.exports = {
  parseStompJs,
};
