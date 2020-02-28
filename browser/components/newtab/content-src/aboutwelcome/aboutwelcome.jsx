/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { HeroText } from "./components/HeroText";
import { FxCards } from "./components/FxCards";
import { DEFAULT_WELCOME_CONTENT } from "../lib/aboutwelcome-utils";

class AboutWelcome extends React.PureComponent {
  render() {
    const { props } = this;
    return (
      <div className="trailheadCards">
        <div className="trailheadCardsInner">
          <HeroText title={props.title} subtitle={props.subtitle} />
          <FxCards
            cards={props.cards}
            sendTelemetry={window.AWSendEventTelemetry}
          />
        </div>
      </div>
    );
  }
}

AboutWelcome.defaultProps = DEFAULT_WELCOME_CONTENT;

function mount(settings) {
  ReactDOM.render(
    <AboutWelcome title={settings.title} subtitle={settings.subtitle} />,
    document.getElementById("root")
  );
}

mount(window.AWGetStartupData());
