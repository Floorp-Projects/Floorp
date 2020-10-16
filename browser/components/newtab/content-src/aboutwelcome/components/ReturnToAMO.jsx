/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  AboutWelcomeUtils,
  DEFAULT_RTAMO_CONTENT,
} from "../../lib/aboutwelcome-utils";
import { Localized } from "./MSLocalized";

export class ReturnToAMO extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClickAddExtension = this.onClickAddExtension.bind(this);
    this.handleStartBtnClick = this.handleStartBtnClick.bind(this);
  }

  onClickAddExtension() {
    const { content, message_id, url } = this.props;
    if (!content?.primary_button?.action?.data) {
      return;
    }

    // Set add-on url in action.data.url property from JSON
    content.primary_button.action.data.url = url;
    AboutWelcomeUtils.handleUserAction(content.primary_button.action);
    const ping = {
      event: "INSTALL",
      event_context: {
        source: "ADD_EXTENSION_BUTTON",
        page: "about:welcome",
      },
      message_id,
    };
    window.AWSendEventTelemetry(ping);
  }

  handleStartBtnClick() {
    const { content, message_id } = this.props;
    AboutWelcomeUtils.handleUserAction(content.startButton.action);
    const ping = {
      event: "CLICK_BUTTON",
      event_context: {
        source: content.startButton.message_id,
        page: "about:welcome",
      },
      message_id,
    };
    window.AWSendEventTelemetry(ping);
  }

  render() {
    const { content } = this.props;
    if (!content) {
      return null;
    }
    // For experiments, when needed below rendered UI allows settings hard coded strings
    // directly inside JSON except for ReturnToAMOText which picks add-on name and icon from fluent string
    return (
      <div className="outer-wrapper onboardingContainer">
        <main className="screen">
          <div className="brand-logo" />
          <div className="welcome-text">
            <Localized text={content.subtitle}>
              <h1 />
            </Localized>
            <Localized text={content.text}>
              <h2
                data-l10n-args={
                  this.props.name
                    ? JSON.stringify({ "addon-name": this.props.name })
                    : null
                }
              >
                <img
                  data-l10n-name="icon"
                  src={this.props.iconURL}
                  role="presentation"
                  alt=""
                />
              </h2>
            </Localized>
            <Localized text={content.primary_button.label}>
              <button onClick={this.onClickAddExtension} className="primary" />
            </Localized>
            <Localized text={content.startButton.label}>
              <button
                onClick={this.handleStartBtnClick}
                className="secondary"
              />
            </Localized>
          </div>
        </main>
      </div>
    );
  }
}

ReturnToAMO.defaultProps = DEFAULT_RTAMO_CONTENT;
