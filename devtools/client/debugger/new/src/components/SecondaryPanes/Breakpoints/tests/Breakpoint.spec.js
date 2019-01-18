/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import Breakpoint from "../Breakpoint";
import { makeSource } from "../../../../utils/test-head";

describe("Breakpoint", () => {
  it("simple", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("disabled", () => {
    const { component } = render({
      breakpoint: makeBreakpoint({ disabled: true })
    });
    expect(component).toMatchSnapshot();
  });

  it("paused at a generatedLocation", () => {
    const { component } = render({
      frame: { selectedLocation: generatedLocation }
    });
    expect(component).toMatchSnapshot();
  });

  it("paused at an original location", () => {
    const { component } = render({
      frame: { selectedLocation: location },
      breakpoint: { selectedLocation: location }
    });

    expect(component).toMatchSnapshot();
  });

  it("paused at a different", () => {
    const { component } = render({
      frame: { selectedLocation: { ...generatedLocation, line: 14 } }
    });
    expect(component).toMatchSnapshot();
  });
});

const generatedLocation = { sourceId: "foo", line: 53, column: 73 };
const location = { sourceId: "foo/original", line: 5, column: 7 };
const selectedLocation = generatedLocation;

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(<Breakpoint.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  return { component, props, defaultState, instance };
}

function makeBreakpoint(overrides = {}) {
  return {
    selectedLocation,
    disabled: false,
    ...overrides
  };
}

function generateDefaults(overrides = { ...overrides, breakpoint: {} }) {
  const source = makeSource("foo");
  const breakpoint = makeBreakpoint(overrides.breakpoint);
  const selectedSource = makeSource("foo");
  return {
    source,
    breakpoint,
    selectedSource,
    frame: null,
    ...overrides
  };
}
