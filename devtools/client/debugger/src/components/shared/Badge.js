/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import PropTypes from "prop-types";
import "./Badge.css";

class Badge extends React.Component {
  constructor(props) {
    super(props);
  }

  static get propTypes() {
    return {
      badgeText: PropTypes.node.isRequired,
    };
  }

  render() {
    return React.createElement(
      "span",
      {
        className: "badge text-white text-center",
      },
      this.props.badgeText
    );
  }
}

export default Badge;
