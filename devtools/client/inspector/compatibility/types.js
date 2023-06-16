/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const browser = {
  // The id of the browser which is defined in MDN compatibility dataset.
  // e.g. "firefox"
  // https://github.com/mdn/browser-compat-data/tree/master/browsers
  id: PropTypes.string.isRequired,
  // The browser name.
  // e.g. "Firefox", "Firefox Android".
  name: PropTypes.string.isRequired,
  // The status of the browser.
  // This should be one of "release", "beta", "nightly", "esr" or undefined.
  status: PropTypes.string,
  // The version of this browser.
  // e.g. "70.0"
  version: PropTypes.string.isRequired,
};

const node = PropTypes.object;

const issue = {
  // Type of this issue. The type should be one of COMPATIBILITY_ISSUE_TYPE.
  type: PropTypes.string.isRequired,
  // The CSS property which caused this issue.
  property: PropTypes.string.isRequired,
  // The url of MDN documentation for the CSS property.
  url: PropTypes.string,
  // The url of the specification for the CSS property.
  specUrl: PropTypes.string,
  // Whether the CSS property is deprecated or not.
  deprecated: PropTypes.bool.isRequired,
  // Whether the CSS property is experimental or not.
  experimental: PropTypes.bool.isRequired,
  // Whether the CSS property is needed prefix to cover all target browsers or not.
  prefixNeeded: PropTypes.bool.isRequired,
  // The browsers which do not support the CSS property.
  unsupportedBrowsers: PropTypes.arrayOf(PropTypes.shape(browser)).isRequired,
  // Nodes that caused this issue. This will be available for top-level target issues only.
  nodes: PropTypes.arrayOf(node),
  // Prefixed properties that the user set.
  aliases: PropTypes.arrayOf(PropTypes.string),
};

exports.browser = browser;
exports.issue = issue;
exports.node = node;
