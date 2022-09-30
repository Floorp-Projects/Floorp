/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getAbbreviatedMimeType,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

class RequestListColumnType extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.mimeType !== nextProps.item.mimeType;
  }

  render() {
    const { mimeType } = this.props.item;
    let abbrevType;

    if (mimeType) {
      abbrevType = getAbbreviatedMimeType(mimeType);
    }

    return dom.td(
      {
        className: "requests-list-column requests-list-type",
        title: mimeType,
      },
      abbrevType
    );
  }
}

module.exports = RequestListColumnType;
