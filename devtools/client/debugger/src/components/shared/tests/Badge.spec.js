/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import Badge from "../Badge";

describe("Badge", () => {
  it("render", () =>
    expect(
      shallow(
        React.createElement(Badge, {
          badgeText: 3,
        })
      )
    ).toMatchSnapshot());
});
