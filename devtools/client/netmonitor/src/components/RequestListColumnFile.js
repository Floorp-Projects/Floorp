/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { propertiesEqual } = require("../utils/request-utils");

const { div, img } = DOM;

const UPDATED_FILE_PROPS = [
  "responseContentDataUri",
  "urlDetails",
];

const RequestListColumnFile = createClass({
  displayName: "RequestListColumnFile",

  propTypes: {
    item: PropTypes.object.isRequired,
    onThumbnailMouseDown: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_FILE_PROPS, this.props.item, nextProps.item);
  },

  render() {
    let {
      item: { responseContentDataUri, urlDetails },
      onThumbnailMouseDown
    } = this.props;

    return (
      div({
        className: "requests-list-column requests-list-file",
        title: urlDetails.unicodeUrl,
      },
        img({
          className: "requests-list-icon",
          src: responseContentDataUri,
          onMouseDown: onThumbnailMouseDown,
        }),
        urlDetails.baseNameWithQuery
      )
    );
  }
});

module.exports = RequestListColumnFile;
