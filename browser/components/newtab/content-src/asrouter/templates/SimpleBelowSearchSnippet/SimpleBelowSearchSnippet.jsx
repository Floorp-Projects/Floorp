/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Button } from "../../components/Button/Button";
import { RichText } from "../../components/RichText/RichText";
import { safeURI } from "../../template-utils";
import { SnippetBase } from "../../components/SnippetBase/SnippetBase";

const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png";
// Alt text placeholder in case the prop from the server isn't available
const ICON_ALT_TEXT = "";

export class SimpleBelowSearchSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  renderText() {
    const { props } = this;
    return props.content.text ? (
      <RichText
        text={props.content.text}
        customElements={this.props.customElements}
        localization_id="text"
        links={props.content.links}
        sendClick={props.sendClick}
      />
    ) : null;
  }

  renderTitle() {
    const { title } = this.props.content;
    return title ? (
      <h3 className={"title title-inline"}>
        {title}
        <br />
      </h3>
    ) : null;
  }

  async onButtonClick() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        id: this.props.UISurface,
      });
    }
    const { button_url } = this.props.content;
    // If button_url is defined handle it as OPEN_URL action
    const type = this.props.content.button_action || (button_url && "OPEN_URL");
    await this.props.onAction({
      type,
      data: { args: this.props.content.button_action_args || button_url },
    });
    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
  }

  _shouldRenderButton() {
    return (
      this.props.content.button_action ||
      this.props.onButtonClick ||
      this.props.content.button_url
    );
  }

  renderButton() {
    const { props } = this;
    if (!this._shouldRenderButton()) {
      return null;
    }

    return (
      <Button
        onClick={props.onButtonClick || this.onButtonClick}
        color={props.content.button_color}
        backgroundColor={props.content.button_background_color}
      >
        {props.content.button_label}
      </Button>
    );
  }

  render() {
    const { props } = this;
    let className = "SimpleBelowSearchSnippet";
    let containerName = "below-search-snippet";

    if (props.className) {
      className += ` ${props.className}`;
    }
    if (this._shouldRenderButton()) {
      className += " withButton";
      containerName += " withButton";
    }

    return (
      <div className={containerName}>
        <div className="snippet-hover-wrapper">
          <SnippetBase
            {...props}
            className={className}
            textStyle={this.props.textStyle}
          >
            <img
              src={safeURI(props.content.icon) || DEFAULT_ICON_PATH}
              className="icon icon-light-theme"
              alt={props.content.icon_alt_text || ICON_ALT_TEXT}
            />
            <img
              src={
                safeURI(props.content.icon_dark_theme || props.content.icon) ||
                DEFAULT_ICON_PATH
              }
              className="icon icon-dark-theme"
              alt={props.content.icon_alt_text || ICON_ALT_TEXT}
            />
            <div className="textContainer">
              {this.renderTitle()}
              <p className="body">{this.renderText()}</p>
              {this.props.extraContent}
            </div>
            {<div className="buttonContainer">{this.renderButton()}</div>}
          </SnippetBase>
        </div>
      </div>
    );
  }
}
