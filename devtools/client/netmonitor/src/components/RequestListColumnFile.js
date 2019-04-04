/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { propertiesEqual } = require("../utils/request-utils");

const UPDATED_FILE_PROPS = [
  "urlDetails",
];

class RequestListColumnFile extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_FILE_PROPS, this.props.item, nextProps.item);
  }

  render() {
    const {
      item: { urlDetails },
    } = this.props;

    const originalFileURL = urlDetails.url;
    const decodedFileURL = urlDetails.unicodeUrl;
    const ORIGINAL_FILE_URL = L10N.getFormatStr("netRequest.originalFileURL.tooltip",
      originalFileURL);
    const DECODED_FILE_URL = L10N.getFormatStr("netRequest.decodedFileURL.tooltip",
      decodedFileURL);
    const requestedFile = urlDetails.baseNameWithQuery;
    const fileToolTip = originalFileURL === decodedFileURL ?
      originalFileURL : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;

    return (
      dom.td({
        className: "requests-list-column requests-list-file",
        title: fileToolTip,
      },
        requestedFile
      )
    );
  }
}

module.exports = RequestListColumnFile;
