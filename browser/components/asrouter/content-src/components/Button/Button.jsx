/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

const ALLOWED_STYLE_TAGS = ["color", "backgroundColor"];

export const Button = props => {
  const style = {};

  // Add allowed style tags from props, e.g. props.color becomes style={color: props.color}
  for (const tag of ALLOWED_STYLE_TAGS) {
    if (typeof props[tag] !== "undefined") {
      style[tag] = props[tag];
    }
  }
  // remove border if bg is set to something custom
  if (style.backgroundColor) {
    style.border = "0";
  }

  return (
    <button
      onClick={props.onClick}
      className={props.className || "ASRouterButton secondary"}
      style={style}
    >
      {props.children}
    </button>
  );
};
