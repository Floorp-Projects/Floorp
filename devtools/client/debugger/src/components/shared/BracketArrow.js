/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import classNames from "classnames";

import "./BracketArrow.css";

const BracketArrow = ({
  orientation,
  left,
  top,
  bottom,
}: {
  orientation: string,
  left: number,
  top: number,
  bottom: number,
}) => {
  return (
    <div
      className={classNames("bracket-arrow", orientation || "up")}
      style={{ left, top, bottom }}
    />
  );
};

export default BracketArrow;
