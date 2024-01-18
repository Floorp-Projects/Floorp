/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

const classnames = require("resource://devtools/client/shared/classnames.js");

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
