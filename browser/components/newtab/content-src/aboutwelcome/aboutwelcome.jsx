/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { MultiStageAboutWelcome } from "./components/MultiStageAboutWelcome";
import { HeroText } from "./components/HeroText";
import { FxCards } from "./components/FxCards";
import { Localized } from "./components/MSLocalized";

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
    window.AWSendEventTelemetry({
      event: "IMPRESSION",
      event_context: {
        source: this.props.UTMTerm,
        page: "about:welcome",
      },
      message_id: this.props.messageId,
    });
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
    // TBD: Refactor to redirect based off template value
    // inside props.template
    // Create SimpleAboutWelcome that renders default about welcome
    // See Bug 1638087
    if (props.screens) {
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
      <div className="outer-wrapper welcomeContainer">
        <div className="welcomeContainerInner">
          <main>
            <HeroText title={props.title} subtitle={props.subtitle} />
            <FxCards
              cards={props.cards}
              metricsFlowUri={this.state.metricsFlowUri}
              sendTelemetry={window.AWSendEventTelemetry}
              utm_term={props.UTMTerm}
            />
            <Localized text={props.startButton.label}>
              <button
                className="start-button"
                onClick={this.handleStartBtnClick}
              />
            </Localized>
          </main>
        </div>
      </div>
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

mount();
