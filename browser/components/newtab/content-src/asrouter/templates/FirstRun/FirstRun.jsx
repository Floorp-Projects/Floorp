/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Interrupt } from "./Interrupt";
import { Triplets } from "./Triplets";
import { BASE_PARAMS } from "./addUtmParams";

// Note: should match the transition time on .trailheadCards in _Trailhead.scss
const TRANSITION_LENGTH = 500;

export const FLUENT_FILES = [
  "branding/brand.ftl",
  "browser/branding/brandings.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
];

export const helpers = {
  addFluent(document) {
    FLUENT_FILES.forEach(file => {
      const link = document.head.appendChild(document.createElement("link"));
      link.href = file;
      link.rel = "localization";
    });
  },
};

export class FirstRun extends React.PureComponent {
  constructor(props) {
    super(props);

    this.didLoadFlowParams = false;

    this.state = {
      didUserClearInterrupt: false,
      didUserClearTriplets: false,
      flowParams: undefined,
    };

    this.closeInterrupt = this.closeInterrupt.bind(this);
    this.closeTriplets = this.closeTriplets.bind(this);

    helpers.addFluent(this.props.document);
    // Update utm campaign parameters by appending channel for
    // differentiating campaign in amplitude
    if (this.props.appUpdateChannel) {
      BASE_PARAMS.utm_campaign += `-${this.props.appUpdateChannel}`;
    }
  }

  get UTMTerm() {
    const { message } = this.props;
    let UTMTerm = message.utm_term || "";

    UTMTerm =
      message.utm_term && message.trailheadTriplet
        ? `${message.utm_term}-${message.trailheadTriplet}`
        : UTMTerm;
    return UTMTerm;
  }

  async fetchFlowParams() {
    const { fxaEndpoint, fetchFlowParams } = this.props;

    if (fxaEndpoint && this.UTMTerm && !this.didLoadFlowParams) {
      this.didLoadFlowParams = true;
      const flowParams = await fetchFlowParams({
        ...BASE_PARAMS,
        entrypoint: "activity-stream-firstrun",
        form_type: "email",
        utm_term: this.UTMTerm,
      });
      this.setState({ flowParams });
    }
  }

  removeHideMain() {
    if (!this.isInterruptVisible) {
      // We need to remove hide-main since we should show it underneath everything that has rendered
      this.props.document.body.classList.remove("hide-main", "welcome");
    }
  }

  // Is there any interrupt content? This is false for new tab triplets.
  get hasInterrupt() {
    const { message } = this.props;
    return Boolean(message && message.content);
  }

  // Are all conditions met for the interrupt to actually be visible?
  // 1. hasInterrupt - Is there interrupt content?
  // 2. state.didUserClearInterrupt - Was it cleared by the user?
  // 3. props.interruptCleared - Was it cleared externally?
  get isInterruptVisible() {
    return (
      this.hasInterrupt &&
      !this.state.didUserClearInterrupt &&
      !this.props.interruptCleared
    );
  }

  componentDidMount() {
    this.fetchFlowParams();
    this.removeHideMain();
  }

  componentDidUpdate() {
    // In case we didn't have FXA info immediately, try again when we receive it.
    this.fetchFlowParams();
    this.removeHideMain();
  }

  closeInterrupt() {
    this.setState({
      didUserClearInterrupt: true,
    });
  }

  closeTriplets() {
    this.setState({ didUserClearTriplets: true });

    // Closing triplets should prevent any future extended triplets from showing up
    setTimeout(() => {
      this.props.onBlockById("EXTENDED_TRIPLETS_1");
    }, TRANSITION_LENGTH);
  }

  render() {
    const { props, state, UTMTerm } = this;
    const {
      sendUserActionTelemetry,
      fxaEndpoint,
      dispatch,
      executeAction,
      message,
    } = props;

    const { didUserClearTriplets, flowParams } = state;

    const hasTriplets = Boolean(message.bundle && message.bundle.length);
    const interrupt = this.hasInterrupt ? message : null;
    const triplets = hasTriplets ? message.bundle : null;
    const isTripletsContainerVisible = hasTriplets && !didUserClearTriplets;

    // Allow 1) falsy to not render a header 2) default welcome 3) custom header
    const tripletsHeaderId =
      message.tripletsHeaderId === undefined
        ? "onboarding-welcome-header"
        : message.tripletsHeaderId;

    return (
      <>
        {this.isInterruptVisible ? (
          <Interrupt
            document={props.document}
            cards={triplets}
            message={interrupt}
            onNextScene={this.closeInterrupt}
            UTMTerm={UTMTerm}
            sendUserActionTelemetry={sendUserActionTelemetry}
            executeAction={executeAction}
            dispatch={dispatch}
            flowParams={flowParams}
            onDismiss={this.closeInterrupt}
            fxaEndpoint={fxaEndpoint}
            onBlockById={props.onBlockById}
          />
        ) : null}
        {hasTriplets ? (
          <Triplets
            document={props.document}
            cards={triplets}
            headerId={tripletsHeaderId}
            showCardPanel={isTripletsContainerVisible}
            showContent={!this.isInterruptVisible}
            hideContainer={this.closeTriplets}
            sendUserActionTelemetry={sendUserActionTelemetry}
            UTMTerm={`${UTMTerm}-card`}
            flowParams={flowParams}
            onAction={executeAction}
            onBlockById={props.onBlockById}
          />
        ) : null}
      </>
    );
  }
}
