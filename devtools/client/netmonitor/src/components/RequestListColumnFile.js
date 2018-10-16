/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { propertiesEqual, getFileName } = require("../utils/request-utils");

const { div } = dom;

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
      item: { urlDetails, cause },
    } = this.props;
    const iconClassList = ["requests-file-type-icon"];

    if (cause && cause.type == "img") {
      iconClassList.push("file-type-image");
    } else {
      iconClassList.push("file-type-general");
    }

    return (
      div({
        className: "requests-list-column requests-list-file",
        title: urlDetails.unicodeUrl
      },
        div({
          className: iconClassList.join(" "),
          title: getFileName(urlDetails.baseNameWithQuery) || urlDetails.unicodeUrl,
        }),
        urlDetails.baseNameWithQuery
      )
    );
  }
}

module.exports = RequestListColumnFile;
