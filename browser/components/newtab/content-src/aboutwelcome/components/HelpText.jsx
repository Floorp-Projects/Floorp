/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
const MS_STRING_PROP = "string_id";

export const HelpText = props => {
  if (!props.text) {
    return null;
  }

  if (props.hasImg) {
    if (typeof props.text === "object" && props.text[MS_STRING_PROP]) {
      return (
        <Localized text={props.text}>
          <p className={`helptext ${props.position}`}>
            <img
              data-l10n-name="help-img"
              className={`helptext-img ${props.position}`}
              src={props.hasImg.src}
              loading={AboutWelcomeUtils.getLoadingStrategyFor(
                props.hasImg.src
              )}
              alt=""
            ></img>
          </p>
        </Localized>
      );
    } else if (typeof props.text === "string") {
      // Add the img at the end of the props.text
      return (
        <p className={`helptext ${props.position}`}>
          {props.text}
          <img
            className={`helptext-img ${props.position} end`}
            src={props.hasImg.src}
            loading={AboutWelcomeUtils.getLoadingStrategyFor(props.hasImg.src)}
            alt=""
          />
        </p>
      );
    }
  } else {
    return (
      <Localized text={props.text}>
        <p className={`helptext ${props.position}`} />
      </Localized>
    );
  }
  return null;
};
