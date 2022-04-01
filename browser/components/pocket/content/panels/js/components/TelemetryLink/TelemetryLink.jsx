/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import panelMessaging from "../../messages";

function TelemetryLink(props) {
  function onClick(event) {
    if (props.onClick) {
      props.onClick(event);
    } else {
      event.preventDefault();
      panelMessaging.sendMessage("PKT_openTabWithUrl", {
        url: event.currentTarget.getAttribute(`href`),
        source: props.source,
        model: props.model,
        position: props.position,
      });
    }
  }

  return (
    <a
      href={props.href}
      onClick={onClick}
      target="_blank"
      className={props.className}
    >
      {props.children}
    </a>
  );
}

export default TelemetryLink;
