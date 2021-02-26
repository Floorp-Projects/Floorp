/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { MultiStageAboutWelcome } from "./components/MultiStageAboutWelcome";
import { ReturnToAMO } from "./components/ReturnToAMO";

import { DEFAULT_WELCOME_CONTENT } from "../lib/aboutwelcome-utils";

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
      />
    );
  }
}

AboutWelcome.defaultProps = DEFAULT_WELCOME_CONTENT;

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
  // Check for override content in pref browser.aboutwelcome.overrideContent
  let aboutWelcomeProps = await window.AWGetWelcomeOverrideContent();
  if (aboutWelcomeProps?.template) {
    let { messageId, UTMTerm } = ComputeTelemetryInfo(aboutWelcomeProps);
    return { aboutWelcomeProps, messageId, UTMTerm };
  }

  // Check for experiment and retrieve content
  const { slug, branch } = await window.AWGetExperimentData();
  aboutWelcomeProps = branch?.feature ? branch.feature.value : {};

  // Check if there is any attribution data, this could take a while to await in series
  // especially when there is an add-on that requires remote lookup
  // Moving RTAMO as part of another screen of multistage is one option to fix the delay
  // as it will allow the initial page to be fast while we fetch attribution data in parallel for a later screen.
  const attribution = await window.AWGetAttributionData();
  if (attribution?.template) {
    aboutWelcomeProps = {
      ...aboutWelcomeProps,
      // If part of an experiment, render experiment template
      template: aboutWelcomeProps?.template
        ? aboutWelcomeProps.template
        : attribution.template,
      ...attribution.extraProps,
    };
  }

  let { messageId, UTMTerm } = ComputeTelemetryInfo(
    aboutWelcomeProps,
    slug,
    branch && branch.slug
  );
  return { aboutWelcomeProps, messageId, UTMTerm };
}

async function mount() {
  let { aboutWelcomeProps, messageId, UTMTerm } = await retrieveRenderContent();
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
