/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const {
  getFormattedTime,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");

const UPDATED_FILE_PROPS = ["urlDetails", "waitingTime"];

class RequestListColumnFile extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      slowLimit: PropTypes.number,
      onWaterfallMouseDown: PropTypes.func,
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
      item: { urlDetails, waitingTime },
      slowLimit,
      onWaterfallMouseDown,
    } = this.props;

    const originalFileURL = urlDetails.url;
    const decodedFileURL = urlDetails.unicodeUrl;
    const ORIGINAL_FILE_URL = L10N.getFormatStr(
      "netRequest.originalFileURL.tooltip",
      originalFileURL
    );
    const DECODED_FILE_URL = L10N.getFormatStr(
      "netRequest.decodedFileURL.tooltip",
      decodedFileURL
    );
    const requestedFile = urlDetails.baseNameWithQuery;
    const fileToolTip =
      originalFileURL === decodedFileURL
        ? originalFileURL
        : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;

    const isSlow = slowLimit > 0 && !!waitingTime && waitingTime > slowLimit;

    return dom.td(
      {
        className: "requests-list-column requests-list-file",
        title: fileToolTip,
      },
      dom.div({}, requestedFile),
      isSlow &&
        dom.div({
          title: L10N.getFormatStr(
            "netmonitor.audits.slowIconTooltip",
            getFormattedTime(waitingTime),
            getFormattedTime(slowLimit)
          ),
          onMouseDown: onWaterfallMouseDown,
          className: "requests-list-slow-button",
        })
    );
  }
}

module.exports = connect(state => ({
  slowLimit: state.ui.slowLimit,
}))(RequestListColumnFile);
