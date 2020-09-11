/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
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
      didUserClearTriplets: false,
      flowParams: undefined,
    };

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

  componentDidMount() {
    this.fetchFlowParams();
  }

  componentDidUpdate() {
    // In case we didn't have FXA info immediately, try again when we receive it.
    this.fetchFlowParams();
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
    const { sendUserActionTelemetry, executeAction, message } = props;

    const { didUserClearTriplets, flowParams } = state;

    const hasTriplets = Boolean(message.bundle && message.bundle.length);
    const triplets = hasTriplets ? message.bundle : null;
    const isTripletsContainerVisible = hasTriplets && !didUserClearTriplets;

    // Allow 1) falsy to not render a header 2) default welcome header 3) custom header
    const tripletsHeaderId =
      message.tripletsHeaderId === undefined
        ? "onboarding-welcome-header"
        : message.tripletsHeaderId;

    return (
      <>
        {hasTriplets ? (
          <Triplets
            document={props.document}
            cards={triplets}
            headerId={tripletsHeaderId}
            showCardPanel={isTripletsContainerVisible}
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
