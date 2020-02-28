/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { addUtmParams } from "../../asrouter/templates/FirstRun/addUtmParams";
import { OnboardingCard } from "../../asrouter/templates/OnboardingMessage/OnboardingMessage";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

export class FxCards extends React.PureComponent {
  onCardAction(action) {
    let { type, data } = action;
    let UTMTerm = "utm_term_separate_welcome";

    if (action.type === "OPEN_URL") {
      let url = new URL(action.data.args);
      addUtmParams(url, UTMTerm);
      data = { ...data, args: url.toString() };
    }

    AboutWelcomeUtils.handleUserAction({ type, data });
  }

  render() {
    const { props } = this;
    return (
      <React.Fragment>
        <div className={`trailheadCardGrid show`}>
          {props.cards.map(card => (
            <OnboardingCard
              key={card.id}
              message={card}
              className="trailheadCard"
              sendUserActionTelemetry={props.sendTelemetry}
              onAction={this.onCardAction}
              UISurface="ABOUT_WELCOME"
              {...card}
            />
          ))}
        </div>
      </React.Fragment>
    );
  }
}
