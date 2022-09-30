/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  LongStringFront,
} = require("resource://devtools/client/fronts/string.js");

/**
 * Fetches the full text of a LongString.
 *
 * @param {DevToolsClient} client
 * @param {object|string} stringGrip: A long string grip. If the param is a simple string,
 *                                    it will be returned as is.
 * @return {Promise<String>} The full string content.
 */
async function getLongStringFullText(client, stringGrip) {
  if (typeof stringGrip !== "object" || stringGrip.type !== "longString") {
    return stringGrip;
  }

  const { initial, length } = stringGrip;
  const longStringFront = new LongStringFront(
    client
    // this.commands.targetCommand.targetFront
  );
  longStringFront.form(stringGrip);
  // The front has to be managed to be able to call the actor method.
  longStringFront.manage(longStringFront);

  const response = await longStringFront.substring(initial.length, length);
  const payload = initial + response;

  longStringFront.destroy();

  return payload;
}

exports.getLongStringFullText = getLongStringFullText;
