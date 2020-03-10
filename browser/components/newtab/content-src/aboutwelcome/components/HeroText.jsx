/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";

export const HeroText = props => {
  return (
    <React.Fragment>
      <Localized text={props.title}>
        <h1 className="welcome-title" />
      </Localized>
      <Localized text={props.subtitle}>
        <h2 className="welcome-subtitle" />
      </Localized>
    </React.Fragment>
  );
};
