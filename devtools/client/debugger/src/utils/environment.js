/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function isNode() {
  try {
    return process.release.name == "node";
  } catch (e) {
    return false;
  }
}

export function isNodeTest() {
  return isNode() && process.env.NODE_ENV != "production";
}
