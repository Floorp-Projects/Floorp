/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { MultiStageAboutWelcome } from "./components/MultiStageAboutWelcome";
import { SimpleAboutWelcome } from "./components/SimpleAboutWelcome";

import {
  AboutWelcomeUtils,
  DEFAULT_WELCOME_CONTENT,
} from "../lib/aboutwelcome-utils";

class AboutWelcome extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { metricsFlowUri: null };
    this.fetchFxAFlowUri = this.fetchFxAFlowUri.bind(this);
    this.handleStartBtnClick = this.handleStartBtnClick.bind(this);
  }

  async fetchFxAFlowUri() {
    this.setState({ metricsFlowUri: await window.AWGetFxAMetricsFlowURI() });
  }

  componentDidMount() {
    this.fetchFxAFlowUri();

    // Record impression with performance data after allowing the page to load
    window.addEventListener(
      "load",
      () => {
        const { domComplete, domInteractive } = performance
          .getEntriesByType("navigation")
          .pop();
        window.AWSendEventTelemetry({
          event: "IMPRESSION",
          event_context: {
            domComplete,
            domInteractive,
            mountStart: performance.getEntriesByName("mount").pop().startTime,
            source: this.props.UTMTerm,
            page: "about:welcome",
          },
          message_id: this.props.messageId,
        });
      },
      { once: true }
    );

    // Captures user has seen about:welcome by setting
    // firstrun.didSeeAboutWelcome pref to true and capturing welcome UI unique messageId
    window.AWSendToParent("SET_WELCOME_MESSAGE_SEEN", this.props.messageId);
  }

  handleStartBtnClick() {
    AboutWelcomeUtils.handleUserAction(this.props.startButton.action);
    const ping = {
      event: "CLICK_BUTTON",
      event_context: {
        source: this.props.startButton.message_id,
        page: "about:welcome",
      },
      message_id: this.props.messageId,
      id: "ABOUT_WELCOME",
    };
    window.AWSendEventTelemetry(ping);
  }

  render() {
    const { props } = this;
    if (props.template === "multistage") {
      return (
        <MultiStageAboutWelcome
          screens={props.screens}
          metricsFlowUri={this.state.metricsFlowUri}
          message_id={props.messageId}
          utm_term={props.UTMTerm}
        />
      );
    }

    return (
      <SimpleAboutWelcome
        metricsFlowUri={this.state.metricsFlowUri}
        message_id={props.messageId}
        utm_term={props.UTMTerm}
        title={props.title}
        subtitle={props.subtitle}
        cards={props.cards}
        startButton={props.startButton}
        handleStartBtnClick={this.handleStartBtnClick}
      />
    );
  }
}

AboutWelcome.defaultProps = DEFAULT_WELCOME_CONTENT;

function ComputeMessageId(experimentId, branchId, settings) {
  let messageId = "ABOUT_WELCOME";
  let UTMTerm = "default";

  if (settings.id && settings.screens) {
    messageId = settings.id.toUpperCase();
  }

  if (experimentId && branchId) {
    UTMTerm = `${experimentId}-${branchId}`.toLowerCase();
  }
  return {
    messageId,
    UTMTerm,
  };
}

async function mount() {
  const { slug, branch } = await window.AWGetStartupData();
  let settings = branch && branch.value ? branch.value : {};

  if (!(branch && branch.value)) {
    // Check for override content in pref browser.aboutwelcome.overrideContent
    settings = await window.AWGetMultiStageScreens();
  }

  let { messageId, UTMTerm } = ComputeMessageId(
    slug,
    branch && branch.slug,
    settings
  );

  ReactDOM.render(
    <AboutWelcome messageId={messageId} UTMTerm={UTMTerm} {...settings} />,
    document.getElementById("root")
  );
}

performance.mark("mount");
mount();
