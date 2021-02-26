/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "../../../aboutwelcome/components/MSLocalized";

export class OnboardingCard extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    const { props } = this;
    const ping = {
      event: "CLICK_BUTTON",
      message_id: props.id,
      id: props.UISurface,
    };
    props.sendUserActionTelemetry(ping);
    props.onAction(props.content.primary_button.action, props.message);
  }

  render() {
    const { content } = this.props;
    const className = this.props.className || "onboardingMessage";
    return (
      <div className={className}>
        <div className={`onboardingMessageImage ${content.icon}`} />
        <div className="onboardingContent">
          <span>
            <Localized text={content.title}>
              <h2 className="onboardingTitle" />
            </Localized>
            <Localized text={content.text}>
              <p className="onboardingText" />
            </Localized>
          </span>
          <span className="onboardingButtonContainer">
            <Localized text={content.primary_button.label}>
              <button
                className="button onboardingButton"
                onClick={this.onClick}
              />
            </Localized>
          </span>
        </div>
      </div>
    );
  }
}
