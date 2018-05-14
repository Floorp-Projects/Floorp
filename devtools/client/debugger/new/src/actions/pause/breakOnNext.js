"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.breakOnNext = breakOnNext;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Debugger breakOnNext command.
 * It's different from the comand action because we also want to
 * highlight the pause icon.
 *
 * @memberof actions/pause
 * @static
 */
function breakOnNext() {
  return async ({
    dispatch,
    client
  }) => {
    await client.breakOnNext();
    return dispatch({
      type: "BREAK_ON_NEXT"
    });
  };
}