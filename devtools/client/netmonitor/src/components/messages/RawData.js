/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

/**
 * Shows raw data of a message.
 */
class RawData extends Component {
  static get propTypes() {
    return {
      payload: PropTypes.string.isRequired,
    };
  }

  render() {
    const { payload } = this.props;
    return dom.textarea({
      className: "message-rawData-payload",
      rows: payload.split(/\n/g).length + 1,
      value: payload,
      readOnly: true,
    });
  }
}

module.exports = RawData;
