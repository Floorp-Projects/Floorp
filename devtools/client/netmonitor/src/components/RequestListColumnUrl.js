/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { td } = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFormattedIPAndPort } = require("../utils/format-utils");
const { propertiesEqual } = require("../utils/request-utils");
const SecurityState = createFactory(require("./SecurityState"));
const UPDATED_FILE_PROPS = ["remoteAddress", "securityState", "urlDetails"];

class RequestListColumnUrl extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(
      UPDATED_FILE_PROPS,
      this.props.item,
      nextProps.item
    );
  }

  render() {
    const {
      item: { urlDetails },
    } = this.props;

    const { item, onSecurityIconMouseDown } = this.props;

    const {
      remoteAddress,
      remotePort,
      urlDetails: { isLocal },
    } = item;

    const title = remoteAddress
      ? ` (${getFormattedIPAndPort(remoteAddress, remotePort)})`
      : "";

    // deals with returning whole url
    const originalURL = urlDetails.url;
    const decodedFileURL = urlDetails.unicodeUrl;
    const ORIGINAL_FILE_URL = L10N.getFormatStr(
      "netRequest.originalFileURL.tooltip",
      originalURL
    );
    const DECODED_FILE_URL = L10N.getFormatStr(
      "netRequest.decodedFileURL.tooltip",
      decodedFileURL
    );
    const urlToolTip =
      originalURL === decodedFileURL
        ? originalURL
        : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;

    return td(
      {
        className: "requests-list-column requests-list-url",
        title: urlToolTip + title,
      },
      SecurityState({ item, onSecurityIconMouseDown, isLocal }),
      originalURL
    );
  }
}

module.exports = RequestListColumnUrl;
