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
  selectInterruptAndTriplets(message = {}) {
    const hasInterrupt = Boolean(message.content);
    const hasTriplets = Boolean(message.bundle && message.bundle.length);
    const UTMTerm = message.utm_term || "";
    return {
      hasTriplets,
      hasInterrupt,
      interrupt: hasInterrupt ? message : null,
      triplets: hasTriplets ? message.bundle : null,
      UTMTerm,
    };
  },

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
      prevMessage: undefined,

      hasInterrupt: false,
      hasTriplets: false,

      interrupt: undefined,
      triplets: undefined,

      isInterruptVisible: false,
      isTripletsContainerVisible: false,
      isTripletsContentVisible: false,

      UTMTerm: "",

      flowParams: undefined,
    };

    this.closeInterrupt = this.closeInterrupt.bind(this);
    this.closeTriplets = this.closeTriplets.bind(this);

    helpers.addFluent(this.props.document);
  }

  static getDerivedStateFromProps(props, state) {
    const { message } = props;
    if (message && message.id !== state.prevMessageId) {
      const {
        hasTriplets,
        hasInterrupt,
        interrupt,
        triplets,
        UTMTerm,
      } = helpers.selectInterruptAndTriplets(message);

      return {
        prevMessageId: message.id,

        hasInterrupt,
        hasTriplets,

        interrupt,
        triplets,

        isInterruptVisible: hasInterrupt,
        isTripletsContainerVisible: hasTriplets,
        isTripletsContentVisible: !(hasInterrupt || !hasTriplets),

        UTMTerm,
      };
    }
    return null;
  }

  async fetchFlowParams() {
    const { fxaEndpoint, fetchFlowParams } = this.props;
    const { UTMTerm } = this.state;
    if (fxaEndpoint && UTMTerm && !this.didLoadFlowParams) {
      this.didLoadFlowParams = true;
      const flowParams = await fetchFlowParams({
        ...BASE_PARAMS,
        entrypoint: "activity-stream-firstrun",
        form_type: "email",
        utm_term: UTMTerm,
      });
      this.setState({ flowParams });
    }
  }

  removeHideMain() {
    if (!this.state.hasInterrupt) {
      // We need to remove hide-main since we should show it underneath everything that has rendered
      this.props.document.body.classList.remove("hide-main", "welcome");
    }
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
    this.setState(prevState => ({
      isInterruptVisible: false,
      isTripletsContainerVisible: prevState.hasTriplets,
      isTripletsContentVisible: prevState.hasTriplets,
    }));
  }

  closeTriplets() {
    this.setState({ isTripletsContainerVisible: false });

    // Closing triplets should prevent any future extended triplets from showing up
    setTimeout(() => {
      this.props.onBlockById("EXTENDED_TRIPLETS_1");
    }, TRANSITION_LENGTH);
  }

  render() {
    const { props } = this;
    const {
      sendUserActionTelemetry,
      fxaEndpoint,
      dispatch,
      executeAction,
    } = props;

    const {
      interrupt,
      triplets,
      isInterruptVisible,
      isTripletsContainerVisible,
      isTripletsContentVisible,
      hasTriplets,
      UTMTerm,
      flowParams,
    } = this.state;

    return (
      <>
        {isInterruptVisible ? (
          <Interrupt
            document={props.document}
            message={interrupt}
            onNextScene={this.closeInterrupt}
            UTMTerm={UTMTerm}
            sendUserActionTelemetry={sendUserActionTelemetry}
            executeAction={executeAction}
            dispatch={dispatch}
            flowParams={flowParams}
            onDismiss={this.closeInterrupt}
            fxaEndpoint={fxaEndpoint}
          />
        ) : null}
        {hasTriplets ? (
          <Triplets
            document={props.document}
            cards={triplets}
            showCardPanel={isTripletsContainerVisible}
            showContent={isTripletsContentVisible}
            hideContainer={this.closeTriplets}
            sendUserActionTelemetry={sendUserActionTelemetry}
            UTMTerm={`${UTMTerm}-card`}
            flowParams={flowParams}
            onAction={executeAction}
          />
        ) : null}
      </>
    );
  }
}
