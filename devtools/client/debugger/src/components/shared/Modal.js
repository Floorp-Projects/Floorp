/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React from "react";
import classnames from "classnames";
import Transition from "react-transition-group/Transition";
import "./Modal.css";

export const transitionTimeout = 50;

export class Modal extends React.Component {
  onClick = e => {
    e.stopPropagation();
  };

  render() {
    const { additionalClass, children, handleClose, status } = this.props;

    return (
      <div className="modal-wrapper" onClick={handleClose}>
        <div
          className={classnames("modal", additionalClass, status)}
          onClick={this.onClick}
        >
          {children}
        </div>
      </div>
    );
  }
}

Modal.contextTypes = {
  shortcuts: PropTypes.object,
};

export default function Slide({
  in: inProp,
  children,
  additionalClass,
  handleClose,
}) {
  return (
    <Transition in={inProp} timeout={transitionTimeout} appear>
      {status => (
        <Modal
          status={status}
          additionalClass={additionalClass}
          handleClose={handleClose}
        >
          {children}
        </Modal>
      )}
    </Transition>
  );
}
