/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import schema from "../../templates/SimpleSnippet/SimpleSnippet.schema.json";

export class SnippetBase extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
    this.onDismissClicked = this.onDismissClicked.bind(this);
    this.setBlockButtonRef = this.setBlockButtonRef.bind(this);
    this.onBlockButtonMouseEnter = this.onBlockButtonMouseEnter.bind(this);
    this.onBlockButtonMouseLeave = this.onBlockButtonMouseLeave.bind(this);
    this.state = { blockButtonHover: false };
  }

  componentDidMount() {
    if (this.blockButtonRef) {
      this.blockButtonRef.addEventListener(
        "mouseenter",
        this.onBlockButtonMouseEnter
      );
      this.blockButtonRef.addEventListener(
        "mouseleave",
        this.onBlockButtonMouseLeave
      );
    }
  }

  componentWillUnmount() {
    if (this.blockButtonRef) {
      this.blockButtonRef.removeEventListener(
        "mouseenter",
        this.onBlockButtonMouseEnter
      );
      this.blockButtonRef.removeEventListener(
        "mouseleave",
        this.onBlockButtonMouseLeave
      );
    }
  }

  setBlockButtonRef(element) {
    this.blockButtonRef = element;
  }

  onBlockButtonMouseEnter() {
    this.setState({ blockButtonHover: true });
  }

  onBlockButtonMouseLeave() {
    this.setState({ blockButtonHover: false });
  }

  onBlockClicked() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "BLOCK",
        id: this.props.UISurface,
      });
    }

    this.props.onBlock();
  }

  onDismissClicked() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "DISMISS",
        id: this.props.UISurface,
      });
    }

    this.props.onDismiss();
  }

  renderDismissButton() {
    if (this.props.footerDismiss) {
      return (
        <div className="footer">
          <div className="footer-content">
            <button
              className="ASRouterButton secondary"
              onClick={this.onDismissClicked}
            >
              {this.props.content.scene2_dismiss_button_text}
            </button>
          </div>
        </div>
      );
    }

    const label =
      this.props.content.block_button_text ||
      schema.properties.block_button_text.default;
    return (
      <button
        className="blockButton"
        title={label}
        aria-label={label}
        onClick={this.onBlockClicked}
        ref={this.setBlockButtonRef}
      />
    );
  }

  render() {
    const { props } = this;
    const { blockButtonHover } = this.state;

    const containerClassName = `SnippetBaseContainer${
      props.className ? ` ${props.className}` : ""
    }${blockButtonHover ? " active" : ""}`;

    return (
      <div className={containerClassName} style={this.props.textStyle}>
        <div className="innerWrapper">{props.children}</div>
        {this.renderDismissButton()}
      </div>
    );
  }
}
