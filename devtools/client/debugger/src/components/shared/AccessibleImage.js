/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import PropTypes from "prop-types";

const classnames = require("devtools/client/shared/classnames.js");

import "./AccessibleImage.css";

const AccessibleImage = props => {
  return React.createElement("span", {
    ...props,
    className: classnames("img", props.className),
  });
};

AccessibleImage.propTypes = {
  className: PropTypes.string.isRequired,
};

export default AccessibleImage;
