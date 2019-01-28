/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow, mount } from "enzyme";
import Frame from "../Frame.js";

import FrameMenu from "../FrameMenu";
jest.mock("../FrameMenu", () => jest.fn());

function render(frameToSelect = {}, overrides = {}, propsOverrides = {}) {
  const defaultFrame = {
    id: 1,
    source: {
      url: "foo-view.js",
      isBlackBoxed: false
    },
    displayName: "renderFoo",
    frameworkGroupingOn: false,
    toggleFrameworkGrouping: jest.fn(),
    library: false,
    location: {
      line: 10
    }
  };
  const frame = { ...defaultFrame, ...overrides };
  const selectedFrame = { ...frame, ...frameToSelect };
  const selectFrame = jest.fn();
  const toggleBlackBox = jest.fn();

  const props = {
    frame,
    selectedFrame,
    copyStackTrace: jest.fn(),
    contextTypes: {},
    selectFrame,
    toggleBlackBox,
    ...propsOverrides
  };
  const component = shallow(<Frame {...props} />);
  return { component, props };
}

describe("Frame", () => {
  it("user frame", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("user frame (not selected)", () => {
    const { component } = render({ id: 2 });
    expect(component).toMatchSnapshot();
  });

  it("library frame", () => {
    const backboneFrame = {
      id: 3,
      source: { url: "backbone.js" },
      displayName: "updateEvents",
      library: "backbone",
      location: {
        line: 12
      }
    };

    const { component } = render({ id: 3 }, backboneFrame);
    expect(component).toMatchSnapshot();
  });

  it("filename only", () => {
    const frame = {
      id: 1,
      source: {
        url: "https://firefox.com/assets/src/js/foo-view.js"
      },
      displayName: "renderFoo",
      location: {
        line: 10
      }
    };

    const component = mount(<Frame frame={frame} />);
    expect(component.text()).toBe("renderFoo foo-view.js:10");
  });

  it("full URL", () => {
    const url = `https://${"a".repeat(100)}.com/assets/src/js/foo-view.js`;
    const frame = {
      id: 1,
      source: {
        url
      },
      displayName: "renderFoo",
      location: {
        line: 10
      }
    };

    const component = mount(<Frame frame={frame} displayFullUrl={true} />);
    expect(component.text()).toBe(`renderFoo ${url}:10`);
  });

  it("getFrameTitle", () => {
    const url = `https://${"a".repeat(100)}.com/assets/src/js/foo-view.js`;
    const frame = {
      id: 1,
      source: {
        url
      },
      displayName: "renderFoo",
      location: {
        line: 10
      }
    };

    const component = shallow(
      <Frame frame={frame} getFrameTitle={x => `Jump to ${x}`} />
    );
    expect(component.prop("title")).toBe(`Jump to ${url}:10`);
    expect(component).toMatchSnapshot();
  });

  describe("mouse events", () => {
    it("does not call FrameMenu when disableContextMenu is true", () => {
      const { component } = render(undefined, undefined, {
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
        props.frame,
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
