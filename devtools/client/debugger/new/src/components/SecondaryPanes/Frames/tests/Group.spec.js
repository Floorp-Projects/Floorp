/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";
import Group from "../Group.js";

import FrameMenu from "../FrameMenu";
jest.mock("../FrameMenu", () => jest.fn());

function render(overrides = {}) {
  const defaultProps = {
    group: [{ displayName: "foo", library: "Back" }],
    selectedFrame: {},
    frameworkGroupingOn: true,
    toggleFrameworkGrouping: jest.fn(),
    selectFrame: jest.fn(),
    copyStackTrace: jest.fn(),
    toggleBlackBox: jest.fn()
  };

  const props = { ...defaultProps, ...overrides };
  const component = shallow(<Group {...props} />, {
    context: { l10n: L10N }
  });
  return { component, props };
}

describe("Group", () => {
  it("displays a group", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("passes the getFrameTitle prop to the Frame components", () => {
    const group = [
      {
        id: 1,
        library: "Back",
        displayName: "renderFoo",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/mahscripts.js"
        }
      },
      {
        id: 2,
        library: "Back",
        displayName: "a",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/back.js"
        }
      },
      {
        id: 3,
        library: "Back",
        displayName: "b",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/back.js"
        }
      }
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
    const group = [
      {
        id: 1,
        library: "Back",
        displayName: "",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/mahscripts.js"
        }
      },
      {
        id: 2,
        library: "Back",
        displayName: "",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/back.js"
        }
      },
      {
        id: 3,
        library: "Back",
        displayName: "",
        location: {
          line: 55
        },
        source: {
          url: "http://myfile.com/back.js"
        }
      }
    ];

    const { component } = render({ group });
    expect(component).toMatchSnapshot();
    component.setState({ expanded: true });
    expect(component).toMatchSnapshot();
  });

  describe("mouse events", () => {
    it("does not call FrameMenu when disableContextMenu is true", () => {
      const { component } = render({
        disableContextMenu: true
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
          toggleBlackBox
        },
        mockEvent
      );
    });
  });
});
