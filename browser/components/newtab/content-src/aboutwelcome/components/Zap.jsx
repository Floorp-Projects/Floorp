/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";
const MS_STRING_PROP = "string_id";

export const Zap = props => {
  if (!props.text) {
    return null;
  }

  if (props.hasZap) {
    if (typeof props.text === "object" && props.text[MS_STRING_PROP]) {
      return (
        <Localized text={props.text}>
          <h1 className="welcomeZap">
            <span data-l10n-name="zap" />
          </h1>
        </Localized>
      );
    } else if (typeof props.text === "string") {
      // Parse string to zap style last word of the props.text
      let titleArray = props.text.split(" ");
      let lastWord = `${titleArray.pop()}`;
      return (
        <h1 className="welcomeZap">
          {titleArray.join(" ").concat(" ")}
          <span>{lastWord}</span>
        </h1>
      );
    }
  } else {
    return (
      <Localized text={props.text}>
        <h1 />
      </Localized>
    );
  }
  return null;
};
