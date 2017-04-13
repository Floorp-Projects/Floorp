/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getAbbreviatedMimeType } = require("../utils/request-utils");

const { div, span } = DOM;

const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};

const RequestListColumnType = createClass({
  displayName: "RequestListColumnType",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.mimeType !== nextProps.item.mimeType;
  },

  render() {
    const { mimeType } = this.props.item;
    let abbrevType;
    if (mimeType) {
      abbrevType = getAbbreviatedMimeType(mimeType);
      abbrevType = CONTENT_MIME_TYPE_ABBREVIATIONS[abbrevType] || abbrevType;
    }

    return (
      div({
        className: "requests-list-subitem requests-list-type",
        title: mimeType,
      },
        span({ className: "subitem-label" }, abbrevType),
      )
    );
  }
});

module.exports = RequestListColumnType;
