/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { HeroText } from "./HeroText";
import { FxCards } from "./FxCards";
import { Localized } from "./MSLocalized";

export class SimpleAboutWelcome extends React.PureComponent {
  render() {
    const { props } = this;
    return (
      <div className="outer-wrapper welcomeContainer">
        <div className="welcomeContainerInner">
          <main>
            <HeroText title={props.title} subtitle={props.subtitle} />
            <FxCards
              cards={props.cards}
              metricsFlowUri={this.props.metricsFlowUri}
              sendTelemetry={window.AWSendEventTelemetry}
              utm_term={this.props.UTMTerm}
            />
            <Localized text={props.startButton.label}>
              <button
                className="start-button"
                onClick={this.props.handleStartBtnClick}
              />
            </Localized>
          </main>
        </div>
      </div>
    );
  }
}
