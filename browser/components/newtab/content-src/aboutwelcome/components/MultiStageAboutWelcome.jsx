/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback } from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

export const MultiStageAboutWelcome = props => {
  const [index, setScreenIndex] = useState(0);
  // Transition to next screen, opening about:home on last screen button CTA
  const handleTransition =
    index < props.screens.length
      ? useCallback(() => setScreenIndex(prevState => prevState + 1), [])
      : AboutWelcomeUtils.handleUserAction({
          type: "OPEN_ABOUT_PAGE",
          data: { args: "home", where: "current" },
        });

  return (
    <React.Fragment>
      <div className={`welcomeCardGrid`}>
        {props.screens.map(screen => {
          return index === screen.order ? (
            <WelcomeScreen
              key={screen.id}
              id={screen.id}
              content={screen.content}
              navigate={handleTransition}
            />
          ) : null;
        })}
      </div>
    </React.Fragment>
  );
};

const WelcomeScreen = props => {
  return (
    <div className={`${props.id}`}>
      <Localized text={props.content.title}>
        <h1 />
      </Localized>
      <Localized text={props.content.primary_button.label}>
        <button onClick={props.navigate} />
      </Localized>
    </div>
  );
};
