/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

export const HeroImage = props => {
  const { height, url, alt } = props;

  if (!url) {
    return null;
  }

  return (
    <div className="hero-image">
      <img
        style={height ? { height } : null}
        src={url}
        loading={AboutWelcomeUtils.getLoadingStrategyFor(url)}
        alt={alt || ""}
        role={alt ? null : "presentation"}
      />
    </div>
  );
};
