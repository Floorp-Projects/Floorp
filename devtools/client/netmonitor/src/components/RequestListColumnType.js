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

const { div } = DOM;

const RequestListColumnType = createClass({
  displayName: "RequestListColumnType",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.mimeType !== nextProps.item.mimeType;
  },

  render() {
    let { mimeType } = this.props.item;
    let abbrevType;

    if (mimeType) {
      abbrevType = getAbbreviatedMimeType(mimeType);
    }

    return (
      div({
        className: "requests-list-column requests-list-type",
        title: mimeType,
      },
        abbrevType
      )
    );
  }
});

module.exports = RequestListColumnType;
