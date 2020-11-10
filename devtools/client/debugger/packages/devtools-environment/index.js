/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const flag = require("./test-flag");

function isNode() {
  return process && process.release && process.release.name == "node";
}

function isNodeTest() {
  return isNode() && process.env.NODE_ENV != "production";
}

function isTesting() {
  return flag.testing;
}

module.exports = {
  isNode,
  isNodeTest,
  isTesting,
};
