/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import PropTypes from "prop-types";
import React from "react";
import type { Node as ReactNode } from "react";
import classnames from "classnames";
import Transition from "react-transition-group/Transition";
import "./Modal.css";

type TransitionStatus = "entering" | "exiting" | "entered" | "exited";

type ModalProps = {
  status: TransitionStatus,
  children?: ReactNode,
  additionalClass?: string,
  handleClose: () => any,
};

export const transitionTimeout = 175;

export class Modal extends React.Component<ModalProps> {
  onClick = (e: SyntheticEvent<HTMLElement>) => {
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

type SlideProps = {
  in: boolean,
  children?: ReactNode,
  additionalClass?: string,
  handleClose: () => any,
};

export default function Slide({
  in: inProp,
  children,
  additionalClass,
  handleClose,
}: SlideProps) {
  return (
    <Transition in={inProp} timeout={transitionTimeout} appear>
      {(status: TransitionStatus) => (
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
