/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class ModalOverlayWrapper extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  // The intended behaviour is to listen for an escape key
  // but not for a click; see Bug 1582242
  onKeyDown(event) {
    if (event.key === "Escape") {
      this.props.onClose(event);
    }
  }

  componentWillMount() {
    this.props.document.addEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.add("modal-open");
    this.header = this.props.document.getElementById(
      "header-asrouter-container"
    );

    if (this.header) {
      this.header.classList.add("modal-scroll");
      this.props.document.getElementById("root").classList.add("modal-height");
    }
  }

  componentWillUnmount() {
    this.props.document.removeEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.remove("modal-open");

    if (this.header) {
      this.header.classList.remove("modal-scroll");
      this.props.document
        .getElementById("root")
        .classList.remove("modal-height");
    }
  }

  render() {
    const { props } = this;
    let className = props.unstyled ? "" : "modalOverlayInner active";
    if (props.innerClassName) {
      className += ` ${props.innerClassName}`;
    }
    return (
      <React.Fragment>
        <div
          className="modalOverlayOuter active"
          onKeyDown={this.onKeyDown}
          role="presentation"
        />
        <div
          className={className}
          aria-labelledby={props.headerId}
          id={props.id}
          role="dialog"
        >
          {props.hasDismissIcon && (
            <button
              className="icon icon-dismiss"
              onClick={props.onClose}
              data-l10n-id="onboarding-cards-dismiss"
            />
          )}
          {props.children}
        </div>
      </React.Fragment>
    );
  }
}

ModalOverlayWrapper.defaultProps = { document: global.document };

export class ModalOverlay extends React.PureComponent {
  render() {
    const { title, button_label } = this.props;
    return (
      <ModalOverlayWrapper onClose={this.props.onDismissBundle}>
        <h2> {title} </h2>
        {this.props.children}
        <div className="footer">
          <button
            className="button primary modalButton"
            onClick={this.props.onDismissBundle}
          >
            {" "}
            {button_label}{" "}
          </button>
        </div>
      </ModalOverlayWrapper>
    );
  }
}
