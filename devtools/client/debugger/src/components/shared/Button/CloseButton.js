/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { button } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import AccessibleImage from "../AccessibleImage";

function CloseButton({ handleClick, buttonClass, tooltip }) {
  return button(
    {
      className: buttonClass ? `close-btn ${buttonClass}` : "close-btn",
      onClick: handleClick,
      title: tooltip,
    },
    React.createElement(AccessibleImage, {
      className: "close",
    })
  );
}

CloseButton.propTypes = {
  buttonClass: PropTypes.string,
  handleClick: PropTypes.func.isRequired,
  tooltip: PropTypes.string,
};

export default CloseButton;
