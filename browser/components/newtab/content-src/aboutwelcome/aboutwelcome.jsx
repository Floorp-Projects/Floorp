/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";
import { HeroText } from "./components/HeroText";
import { FxCards } from "./components/FxCards";
import { DEFAULT_WELCOME_CONTENT } from "../lib/aboutwelcome-utils";

class AboutWelcome extends React.PureComponent {
  sendTelemetry(ping) {
    // TBD: Handle telemetry messages
  }

  render() {
    const { props } = this;
    return (
      <div className="trailheadCards">
        <div className="trailheadCardsInner">
          <HeroText title={props.title} subtitle={props.subtitle} />
          <FxCards cards={props.cards} sendTelemetry={this.sendTelemetry} />
        </div>
      </div>
    );
  }
}

AboutWelcome.defaultProps = DEFAULT_WELCOME_CONTENT;

ReactDOM.render(<AboutWelcome />, document.getElementById("root"));
