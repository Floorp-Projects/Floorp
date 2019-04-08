/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import Breakpoint from "../Breakpoint";
import { makeSource, makeOriginalSource } from "../../../../utils/test-head";

describe("Breakpoint", () => {
  it("simple", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("disabled", () => {
    const { component } = render({}, makeBreakpoint({ disabled: true }));
    expect(component).toMatchSnapshot();
  });

  it("paused at a generatedLocation", () => {
    const { component } = render({
      frame: { selectedLocation: generatedLocation }
    });
    expect(component).toMatchSnapshot();
  });

  it("paused at an original location", () => {
    const { component } = render(
      {
        selectedSource: makeOriginalSource("foo"),
        frame: { selectedLocation: location }
      },
      { location, options: {} }
    );

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

function render(overrides = {}, breakpointOverrides = {}) {
  const props = generateDefaults(overrides, breakpointOverrides);
  // $FlowIgnore
  const component = shallow(<Breakpoint.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  return { component, props, defaultState, instance };
}

function makeBreakpoint(overrides = {}) {
  return {
    location,
    generatedLocation,
    disabled: false,
    options: {},
    ...overrides,
    id: 1
  };
}

function generateDefaults(overrides = {}, breakpointOverrides = {}) {
  const source = makeSource("foo");
  const breakpoint = makeBreakpoint(breakpointOverrides);
  const selectedSource = makeSource("foo");
  return {
    source,
    breakpoint,
    selectedSource,
    frame: (null: any),
    ...overrides
  };
}
