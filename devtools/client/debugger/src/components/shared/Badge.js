/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React from "react";
import "./Badge.css";

type Props = {
  children: number,
};

const Badge = ({ children }: Props) => (
  <span className="badge text-white text-center">{children}</span>
);

export default Badge;
