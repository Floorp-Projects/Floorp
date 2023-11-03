/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React from "react";
import { div } from "react-dom-factories";
const classnames = require("devtools/client/shared/classnames.js");
import "./Modal.css";

class Modal extends React.Component {
  static get propTypes() {
    return {
      additionalClass: PropTypes.string,
      children: PropTypes.node.isRequired,
      handleClose: PropTypes.func.isRequired,
    };
  }

  onClick = e => {
    e.stopPropagation();
  };

  render() {
    const { additionalClass, children, handleClose } = this.props;
    return div(
      {
        className: "modal-wrapper",
        onClick: handleClose,
      },
      div(
        {
          className: classnames("modal", additionalClass),
          onClick: this.onClick,
        },
        children
      )
    );
  }
}

Modal.contextTypes = {
  shortcuts: PropTypes.object,
};

export default Modal;
