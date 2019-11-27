/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { addUtmParams } from "../FirstRun/addUtmParams";
import { FxASignupForm } from "../../components/FxASignupForm/FxASignupForm";
import { OnboardingCard } from "../../templates/OnboardingMessage/OnboardingMessage";
import React from "react";

export const FxAccounts = ({
  document,
  content,
  dispatch,
  fxaEndpoint,
  flowParams,
  removeOverlay,
  url,
  UTMTerm,
}) => (
  <React.Fragment>
    <div
      className="fullpage-left-section"
      aria-labelledby="fullpage-left-title"
      aria-describedby="fullpage-left-content"
    >
      <h1
        id="fullpage-left-title"
        className="fullpage-left-title"
        data-l10n-id="onboarding-welcome-body"
      />
      <p
        id="fullpage-left-content"
        className="fullpage-left-content"
        data-l10n-id="onboarding-benefit-products-text"
      />
      <p
        className="fullpage-left-content"
        data-l10n-id="onboarding-benefit-privacy-text"
      />
      <a
        className="fullpage-left-link"
        href={addUtmParams(url, UTMTerm)}
        target="_blank"
        rel="noopener noreferrer"
        data-l10n-id="onboarding-welcome-learn-more"
      />
      <div className="fullpage-icon fx-systems-icons" />
    </div>
    <div className="fullpage-form">
      <FxASignupForm
        document={document}
        content={content}
        dispatch={dispatch}
        fxaEndpoint={fxaEndpoint}
        UTMTerm={UTMTerm}
        flowParams={flowParams}
        onClose={removeOverlay}
        showSignInLink={true}
      />
    </div>
  </React.Fragment>
);

export const FxCards = ({ cards, onCardAction, sendUserActionTelemetry }) => (
  <React.Fragment>
    {cards.map(card => (
      <OnboardingCard
        key={card.id}
        message={card}
        className="trailheadCard"
        sendUserActionTelemetry={sendUserActionTelemetry}
        onAction={onCardAction}
        UISurface="TRAILHEAD"
        {...card}
      />
    ))}
  </React.Fragment>
);

export class FullPageInterrupt extends React.PureComponent {
  constructor(props) {
    super(props);
    this.removeOverlay = this.removeOverlay.bind(this);
    this.onCardAction = this.onCardAction.bind(this);
  }

  componentWillMount() {
    global.document.body.classList.add("trailhead-fullpage");
  }

  componentDidMount() {
    // Hide the page content from screen readers while the full page interrupt is open
    this.props.document
      .getElementById("root")
      .setAttribute("aria-hidden", "true");
  }

  removeOverlay() {
    window.removeEventListener("visibilitychange", this.removeOverlay);
    document.body.classList.remove("hide-main", "trailhead-fullpage");
    // Re-enable the document for screen readers
    this.props.document
      .getElementById("root")
      .setAttribute("aria-hidden", "false");

    this.props.onBlock();
    document.body.classList.remove("welcome");
  }

  onCardAction(action, message) {
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
    // Only block if message is in dynamic triplets experiment
    if (message.blockOnClick) {
      this.props.onBlockById(message.id, { preloadedOnly: true });
    }
    this.removeOverlay();
  }

  render() {
    const { props } = this;
    const { content } = props.message;
    const cards = (
      <FxCards
        cards={props.cards}
        onCardAction={this.onCardAction}
        sendUserActionTelemetry={props.sendUserActionTelemetry}
      />
    );

    const accounts = (
      <FxAccounts
        document={props.document}
        content={content}
        dispatch={props.dispatch}
        fxaEndpoint={props.fxaEndpoint}
        flowParams={props.flowParams}
        removeOverlay={this.removeOverlay}
        url={content.learn.url}
        UTMTerm={props.UTMTerm}
      />
    );

    // By default we show accounts section on top and
    // cards section in bottom half of the full page interrupt
    const cardsFirst = content && content.className === "fullPageCardsAtTop";
    const firstContainerClassName = [
      "container",
      content && content.className,
    ].join(" ");
    return (
      <div className="fullpage-wrapper">
        <div className="fullpage-icon brand-logo" />
        <h1
          className="welcome-title"
          data-l10n-id="onboarding-welcome-header"
        />
        <h2
          className="welcome-subtitle"
          data-l10n-id="onboarding-fullpage-welcome-subheader"
        />
        <div className={firstContainerClassName}>
          {cardsFirst ? cards : accounts}
        </div>
        <div className="section-divider" />
        <div className="container">{cardsFirst ? accounts : cards}</div>
      </div>
    );
  }
}

FullPageInterrupt.defaultProps = {
  flowParams: { deviceId: "", flowId: "", flowBeginTime: "" },
};
