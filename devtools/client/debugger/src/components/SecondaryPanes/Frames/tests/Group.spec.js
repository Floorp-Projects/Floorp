/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import Group from "../Group.js";
import {
  makeMockFrame,
  makeMockSource,
  mockthreadcx,
} from "../../../../utils/test-mockup";

import FrameMenu from "../FrameMenu";
jest.mock("../FrameMenu", () => jest.fn());

function render(overrides = {}) {
  const frame = { ...makeMockFrame(), displayName: "foo", library: "Back" };
  const defaultProps = {
    cx: mockthreadcx,
    group: [frame],
    selectedFrame: frame,
    frameworkGroupingOn: true,
    toggleFrameworkGrouping: jest.fn(),
    selectFrame: jest.fn(),
    copyStackTrace: jest.fn(),
    toggleBlackBox: jest.fn(),
    disableContextMenu: false,
    displayFullUrl: false,
    selectable: true,
  };

  const props = { ...defaultProps, ...overrides };
  const component = shallow(<Group {...props} />, {
    context: { l10n: L10N },
  });
  return { component, props };
}

describe("Group", () => {
  it("displays a group", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("passes the getFrameTitle prop to the Frame components", () => {
    const mahscripts = makeMockSource("http://myfile.com/mahscripts.js");
    const back = makeMockSource("http://myfile.com/back.js");
    const group = [
      {
        ...makeMockFrame("1", mahscripts, undefined, 55, "renderFoo"),
        library: "Back",
      },
      {
        ...makeMockFrame("2", back, undefined, 55, "a"),
        library: "Back",
      },
      {
        ...makeMockFrame("3", back, undefined, 55, "b"),
        library: "Back",
      },
    ];
    const getFrameTitle = () => {};
    const { component } = render({ group, getFrameTitle });

    component.setState({ expanded: true });

    const frameComponents = component.find("Frame");
    expect(frameComponents).toHaveLength(3);
    frameComponents.forEach(node => {
      expect(node.prop("getFrameTitle")).toBe(getFrameTitle);
    });
    expect(component).toMatchSnapshot();
  });

  it("renders group with anonymous functions", () => {
    const mahscripts = makeMockSource("http://myfile.com/mahscripts.js");
    const back = makeMockSource("http://myfile.com/back.js");
    const group = [
      {
        ...makeMockFrame("1", mahscripts, undefined, 55),
        library: "Back",
      },
      {
        ...makeMockFrame("2", back, undefined, 55),
        library: "Back",
      },
      {
        ...makeMockFrame("3", back, undefined, 55),
        library: "Back",
      },
    ];

    const { component } = render({ group });
    expect(component).toMatchSnapshot();
    component.setState({ expanded: true });
    expect(component).toMatchSnapshot();
  });

  describe("mouse events", () => {
    it("does not call FrameMenu when disableContextMenu is true", () => {
      const { component } = render({
        disableContextMenu: true,
      });

      const mockEvent = "mockEvent";
      component.simulate("contextmenu", mockEvent);

      expect(FrameMenu).toHaveBeenCalledTimes(0);
    });

    it("calls FrameMenu on right click", () => {
      const { component, props } = render();
      const { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox } = props;
      const mockEvent = "mockEvent";
      component.simulate("contextmenu", mockEvent);

      expect(FrameMenu).toHaveBeenCalledWith(
        props.group[0],
        props.frameworkGroupingOn,
        {
          copyStackTrace,
          toggleFrameworkGrouping,
          toggleBlackBox,
        },
        mockEvent
      );
    });
  });
});
