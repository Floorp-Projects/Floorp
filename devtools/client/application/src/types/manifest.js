/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  MANIFEST_ISSUE_LEVELS,
} = require("resource://devtools/client/application/src/constants.js");
const {
  MANIFEST_MEMBER_VALUE_TYPES,
} = require("resource://devtools/client/application/src/constants.js");

const manifestIssue = {
  level: PropTypes.oneOf(Object.values(MANIFEST_ISSUE_LEVELS)).isRequired,
  message: PropTypes.string.isRequired,
  // NOTE: we are currently ignoring the 'type' field that platform adds to errors
};

const manifestIssueArray = PropTypes.arrayOf(PropTypes.shape(manifestIssue));

const manifestItemColor = {
  label: PropTypes.string.isRequired,
  value: PropTypes.string,
};

const manifestItemIcon = {
  label: PropTypes.shape({
    contentType: PropTypes.string,
    sizes: PropTypes.string,
  }).isRequired,
  value: PropTypes.shape({
    src: PropTypes.string.isRequired,
    purpose: PropTypes.string.isRequired,
  }).isRequired,
};

const manifestItemUrl = {
  label: PropTypes.string.isRequired,
  value: PropTypes.string,
};

const manifestMemberColor = {
  key: manifestItemColor.label,
  value: manifestItemColor.value,
  type: PropTypes.oneOf([MANIFEST_MEMBER_VALUE_TYPES.COLOR]),
};

const manifestMemberIcon = {
  key: manifestItemIcon.label,
  value: manifestItemIcon.value,
  type: PropTypes.oneOf([MANIFEST_MEMBER_VALUE_TYPES.ICON]),
};

const manifestMemberString = {
  key: PropTypes.string.isRequired,
  value: PropTypes.string,
  type: PropTypes.oneOf([MANIFEST_MEMBER_VALUE_TYPES.STRING]),
};

const manifest = {
  // members
  identity: PropTypes.arrayOf(PropTypes.shape(manifestMemberString)).isRequired,
  presentation: PropTypes.arrayOf(
    PropTypes.oneOfType([
      PropTypes.shape(manifestMemberColor),
      PropTypes.shape(manifestMemberString),
    ])
  ).isRequired,
  icons: PropTypes.arrayOf(PropTypes.shape(manifestMemberIcon)).isRequired,
  // validation issues
  validation: manifestIssueArray.isRequired,
  // misc
  url: PropTypes.string.isRequired,
};

module.exports = {
  // full manifest
  manifest,
  // specific manifest items
  manifestItemColor,
  manifestItemIcon,
  manifestItemUrl,
  // manifest issues
  manifestIssue,
  manifestIssueArray,
};
