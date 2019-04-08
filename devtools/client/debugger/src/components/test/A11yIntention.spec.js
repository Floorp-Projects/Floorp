/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import A11yIntention from "../A11yIntention";

function render() {
  return shallow(
    <A11yIntention>
      <span>hello world</span>
    </A11yIntention>
  );
}

describe("A11yIntention", () => {
  it("renders its children", () => {
    const component = render();
    expect(component).toMatchSnapshot();
  });

  it("indicates that the mouse or keyboard is being used", () => {
    const component = render();
    expect(component.prop("className")).toEqual("A11y-mouse");

    component.simulate("keyDown");
    expect(component.prop("className")).toEqual("A11y-keyboard");

    component.simulate("mouseDown");
    expect(component.prop("className")).toEqual("A11y-mouse");
  });
});
