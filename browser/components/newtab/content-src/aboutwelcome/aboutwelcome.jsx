/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { MultiStageAboutWelcome } from "./components/MultiStageAboutWelcome";
import { ReturnToAMO } from "./components/ReturnToAMO";

class AboutWelcome extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { metricsFlowUri: null };
    this.fetchFxAFlowUri = this.fetchFxAFlowUri.bind(this);
  }

  async fetchFxAFlowUri() {
    this.setState({ metricsFlowUri: await window.AWGetFxAMetricsFlowURI() });
  }

  componentDidMount() {
    this.fetchFxAFlowUri();

    // Rely on shared proton in-content styling for consistency.
    if (this.props.design === "proton") {
      const sheet = document.head.appendChild(document.createElement("link"));
      sheet.rel = "stylesheet";
      sheet.href = "chrome://global/skin/in-content/common.css";
    }

    // Record impression with performance data after allowing the page to load
    const recordImpression = domState => {
      const { domComplete, domInteractive } = performance
        .getEntriesByType("navigation")
        .pop();
      window.AWSendEventTelemetry({
        event: "IMPRESSION",
        event_context: {
          domComplete,
          domInteractive,
          mountStart: performance.getEntriesByName("mount").pop().startTime,
          domState,
          source: this.props.UTMTerm,
          page: "about:welcome",
        },
        message_id: this.props.messageId,
      });
    };
    if (document.readyState === "complete") {
      // Page might have already triggered a load event because it waited for async data,
      // e.g., attribution, so the dom load timing could be of a empty content
      // with domState in telemetry captured as 'complete'
      recordImpression(document.readyState);
    } else {
      window.addEventListener("load", () => recordImpression("load"), {
        once: true,
      });
    }

    // Captures user has seen about:welcome by setting
    // firstrun.didSeeAboutWelcome pref to true and capturing welcome UI unique messageId
    window.AWSendToParent("SET_WELCOME_MESSAGE_SEEN", this.props.messageId);
  }

  render() {
    const { props } = this;
    if (props.template === "return_to_amo") {
      return (
        <ReturnToAMO
          message_id={props.messageId}
          name={props.name}
          url={props.url}
          iconURL={props.iconURL}
        />
      );
    }

    return (
      <MultiStageAboutWelcome
        screens={props.screens}
        metricsFlowUri={this.state.metricsFlowUri}
        message_id={props.messageId}
        utm_term={props.UTMTerm}
        design={props.design}
        background_url={props.background_url}
      />
    );
  }
}

// Computes messageId and UTMTerm info used in telemetry
function ComputeTelemetryInfo(welcomeContent, experimentId, branchId) {
  let messageId =
    welcomeContent.template === "return_to_amo"
      ? "RTAMO_DEFAULT_WELCOME"
      : "DEFAULT_ABOUTWELCOME";
  let UTMTerm = "default";

  if (welcomeContent.id) {
    messageId = welcomeContent.id.toUpperCase();
  }

  if (experimentId && branchId) {
    UTMTerm = `${experimentId}-${branchId}`.toLowerCase();
  }
  return {
    messageId,
    UTMTerm,
  };
}

async function retrieveRenderContent() {
  // Feature config includes:
  // user prefs
  // experiment data
  // attribution data
  // defaults
  let featureConfig = await window.AWGetFeatureConfig();

  let { messageId, UTMTerm } = ComputeTelemetryInfo(
    featureConfig,
    featureConfig.slug,
    featureConfig.branch && featureConfig.branch.slug
  );
  return { featureConfig, messageId, UTMTerm };
}

async function mount() {
  let {
    featureConfig: aboutWelcomeProps,
    messageId,
    UTMTerm,
  } = await retrieveRenderContent();
  ReactDOM.render(
    <AboutWelcome
      messageId={messageId}
      UTMTerm={UTMTerm}
      {...aboutWelcomeProps}
    />,
    document.getElementById("root")
  );
}

performance.mark("mount");
mount();
