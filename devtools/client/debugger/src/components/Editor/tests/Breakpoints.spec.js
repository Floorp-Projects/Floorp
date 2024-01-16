/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";
import Breakpoints from "../Breakpoints";

const BreakpointsComponent = Breakpoints.WrappedComponent;

function generateDefaults(overrides) {
  const sourceId = "server1.conn1.child1/source1";
  const matchingBreakpoints = [{ location: { source: { id: sourceId } } }];

  return {
    selectedSource: { sourceId, get: () => false },
    editor: {
      codeMirror: {
        setGutterMarker: jest.fn(),
      },
    },
    blackboxedRanges: {},
    breakpointActions: {},
    editorActions: {},
    breakpoints: matchingBreakpoints,
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(React.createElement(BreakpointsComponent, props));
  return { component, props };
}

describe("Breakpoints Component", () => {
  it("should render breakpoints without columns", async () => {
    const sourceId = "server1.conn1.child1/source1";
    const breakpoints = [{ location: { source: { id: sourceId } } }];

    const { component, props } = render({ breakpoints });
    expect(component.find("Breakpoint")).toHaveLength(props.breakpoints.length);
  });

  it("should render breakpoints with columns", async () => {
    const sourceId = "server1.conn1.child1/source1";
    const breakpoints = [{ location: { column: 2, source: { id: sourceId } } }];

    const { component, props } = render({ breakpoints });
    expect(component.find("Breakpoint")).toHaveLength(props.breakpoints.length);
    expect(component).toMatchSnapshot();
  });
});
