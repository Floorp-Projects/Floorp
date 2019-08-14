/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { OnboardingCard } from "../../templates/OnboardingMessage/OnboardingMessage";
import { addUtmParams } from "./addUtmParams";

export class Triplets extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onCardAction = this.onCardAction.bind(this);
    this.onHideContainer = this.onHideContainer.bind(this);
  }

  componentWillMount() {
    global.document.body.classList.add("inline-onboarding");
  }

  componentWillUnmount() {
    this.props.document.body.classList.remove("inline-onboarding");
  }

  onCardAction(action) {
    let actionUpdates = {};
    const { flowParams, UTMTerm } = this.props;

    if (action.type === "OPEN_URL") {
      let url = new URL(action.data.args);
      addUtmParams(url, UTMTerm);

      if (action.addFlowParams) {
        url.searchParams.append("device_id", flowParams.deviceId);
        url.searchParams.append("flow_id", flowParams.flowId);
        url.searchParams.append("flow_begin_time", flowParams.flowBeginTime);
      }

      actionUpdates = { data: { ...action.data, args: url.toString() } };
    }

    this.props.onAction({ ...action, ...actionUpdates });
  }

  onHideContainer() {
    const { sendUserActionTelemetry, cards, hideContainer } = this.props;
    hideContainer();
    sendUserActionTelemetry({
      event: "DISMISS",
      id: "onboarding-cards",
      message_id: cards.map(m => m.id).join(","),
      action: "onboarding_user_event",
    });
  }

  render() {
    const {
      cards,
      showCardPanel,
      showContent,
      sendUserActionTelemetry,
    } = this.props;
    return (
      <div
        className={`trailheadCards ${showCardPanel ? "expanded" : "collapsed"}`}
      >
        <div className="trailheadCardsInner" aria-hidden={!showContent}>
          <h1 data-l10n-id="onboarding-welcome-header" />
          <div className={`trailheadCardGrid${showContent ? " show" : ""}`}>
            {cards.map(card => (
              <OnboardingCard
                key={card.id}
                className="trailheadCard"
                sendUserActionTelemetry={sendUserActionTelemetry}
                onAction={this.onCardAction}
                UISurface="TRAILHEAD"
                {...card}
              />
            ))}
          </div>
          {showCardPanel && (
            <button
              className="icon icon-dismiss"
              onClick={this.onHideContainer}
              data-l10n-id="onboarding-cards-dismiss"
            />
          )}
        </div>
      </div>
    );
  }
}
