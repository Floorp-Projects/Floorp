/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
import InlineSVG from "svg-inline-react";

const svg = {
  breakpoint: require("./breakpoint.svg"),
  "column-marker": require("./column-marker.svg")
};

type SvgType = {
  name: string,
  className?: string,
  onClick?: () => void,
  "aria-label"?: string
};

function Svg({ name, className, onClick, "aria-label": ariaLabel }: SvgType) {
  if (!svg[name]) {
    const error = `Unknown SVG: ${name}`;
    console.warn(error);
    return null;
  }

  const props = {
    className: `${name} ${className || ""}`,
    onClick,
    "aria-label": ariaLabel,
    src: svg[name]
  };

  return <InlineSVG {...props} />;
}

Svg.displayName = "Svg";

module.exports = Svg;
