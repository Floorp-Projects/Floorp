/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { div } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

const classnames = require("resource://devtools/client/shared/classnames.js");

const BracketArrow = ({ orientation, left, top, bottom }) => {
  return div({
    className: classnames("bracket-arrow", orientation || "up"),
    style: {
      left,
      top,
      bottom,
    },
  });
};

BracketArrow.propTypes = {
  bottom: PropTypes.number,
  left: PropTypes.number,
  orientation: PropTypes.string.isRequired,
  top: PropTypes.number,
};

export default BracketArrow;
