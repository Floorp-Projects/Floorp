/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export const HeroText = props => {
  return (
    <React.Fragment>
      <h1 className="welcome-title" data-l10n-id={props.title.string_id} />
      {props.subtitle && props.subtitle.string_id && (
        <h2
          className="welcome-subtitle"
          data-l10n-id={props.subtitle.string_id}
        />
      )}
    </React.Fragment>
  );
};
