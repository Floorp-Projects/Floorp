/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function Button(props) {
  return (
    <a
      href={props.url}
      onClick={props.onClick}
      className={`stp_button${props?.style && ` stp_button_${props.style}`}`}
    >
      {props.children}
    </a>
  );
}

export default Button;
