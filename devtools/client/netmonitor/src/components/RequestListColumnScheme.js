/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div } = DOM;

class RequestListColumnScheme extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.urlDetails.scheme !== nextProps.item.urlDetails.scheme;
  }

  render() {
    const { urlDetails } = this.props.item;
    return (
      div({
        className: "requests-list-column requests-list-scheme",
        title: urlDetails.scheme
      },
        urlDetails.scheme
      )
    );
  }
}

module.exports = RequestListColumnScheme;
