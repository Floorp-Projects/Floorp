/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { addUtmParams } from "../../asrouter/templates/FirstRun/addUtmParams";
import { OnboardingCard } from "../../asrouter/templates/OnboardingMessage/OnboardingMessage";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

export class FxCards extends React.PureComponent {
  constructor(props) {
    super(props);

    this.state = { flowParams: null };

    this.fetchFxAFlowParams = this.fetchFxAFlowParams.bind(this);
    this.onCardAction = this.onCardAction.bind(this);
  }

  componentDidUpdate() {
    this.fetchFxAFlowParams();
  }

  componentDidMount() {
    this.fetchFxAFlowParams();
  }

  async fetchFxAFlowParams() {
    if (this.state.flowParams || !this.props.metricsFlowUri) {
      return;
    }

    let flowParams;
    try {
      const response = await fetch(this.props.metricsFlowUri, {
        credentials: "omit",
      });
      if (response.status === 200) {
        const { deviceId, flowId, flowBeginTime } = await response.json();
        flowParams = { deviceId, flowId, flowBeginTime };
      } else {
        console.error("Non-200 response", response); // eslint-disable-line no-console
      }
    } catch (e) {
      flowParams = null;
    }

    this.setState({ flowParams });
  }

  onCardAction(action) {
    let { type, data } = action;
    let UTMTerm = `aboutwelcome-${this.props.utm_term}-card`;

    if (action.type === "OPEN_URL") {
      let url = new URL(action.data.args);
      addUtmParams(url, UTMTerm);

      if (action.addFlowParams && this.state.flowParams) {
        url.searchParams.append("device_id", this.state.flowParams.deviceId);
        url.searchParams.append("flow_id", this.state.flowParams.flowId);
        url.searchParams.append(
          "flow_begin_time",
          this.state.flowParams.flowBeginTime
        );
      }

      data = { ...data, args: url.toString() };
    }

    AboutWelcomeUtils.handleUserAction({ type, data });
  }

  render() {
    const { props } = this;
    return (
      <React.Fragment>
        <div className={`welcomeCardGrid show`}>
          {props.cards.map(card => (
            <OnboardingCard
              key={card.id}
              message={card}
              className="welcomeCard"
              sendUserActionTelemetry={props.sendTelemetry}
              onAction={this.onCardAction}
              UISurface="ABOUT_WELCOME"
              {...card}
            />
          ))}
        </div>
      </React.Fragment>
    );
  }
}
