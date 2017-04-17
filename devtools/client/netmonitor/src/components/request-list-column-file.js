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
  "urlDetails",
  "responseContentDataUri",
];

const RequestListColumnFile = createClass({
  displayName: "RequestListColumnFile",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_FILE_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { urlDetails, responseContentDataUri } = this.props.item;

    return (
      div({ className: "requests-list-subitem requests-list-icon-and-file" },
        img({
          className: "requests-list-icon",
          src: responseContentDataUri,
          hidden: !responseContentDataUri,
        }),
        div({
          className: "subitem-label requests-list-file",
          title: urlDetails.unicodeUrl,
        },
          urlDetails.baseNameWithQuery,
        ),
      )
    );
  }
});

module.exports = RequestListColumnFile;
