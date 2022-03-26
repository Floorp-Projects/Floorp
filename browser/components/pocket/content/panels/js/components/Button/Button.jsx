/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import TelemetryLink from "../TelemetryLink/TelemetryLink";

function Button(props) {
  return (
    <TelemetryLink
      href={props.url}
      onClick={props.onClick}
      className={`stp_button${props?.style && ` stp_button_${props.style}`}`}
      source={props.source}
    >
      {props.children}
    </TelemetryLink>
  );
}

export default Button;
