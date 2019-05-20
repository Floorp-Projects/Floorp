/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow, mount } from "enzyme";
import Frame from "../Frame.js";
import {
  makeMockFrame,
  makeMockSource,
  mockthreadcx,
} from "../../../../utils/test-mockup";

import FrameMenu from "../FrameMenu";
jest.mock("../FrameMenu", () => jest.fn());

function frameProperties(frame, selectedFrame: any, overrides = {}) {
  return {
    cx: mockthreadcx,
    frame,
    selectedFrame,
    copyStackTrace: jest.fn(),
    contextTypes: {},
    selectFrame: jest.fn(),
    toggleBlackBox: jest.fn(),
    displayFullUrl: false,
    frameworkGroupingOn: false,
    selectable: true,
    toggleFrameworkGrouping: null,
    ...overrides,
  };
}

function render(frameToSelect = {}, overrides = {}, propsOverrides = {}) {
  const source = makeMockSource("foo-view.js");
  const defaultFrame = makeMockFrame("1", source, undefined, 10, "renderFoo");

  const frame = { ...defaultFrame, ...overrides };
  const selectedFrame = { ...frame, ...frameToSelect };

  const props = frameProperties(frame, selectedFrame, propsOverrides);
  const component = shallow(<Frame {...props} />);
  return { component, props };
}

describe("Frame", () => {
  it("user frame", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("user frame (not selected)", () => {
    const { component } = render({ id: "2" });
    expect(component).toMatchSnapshot();
  });

  it("library frame", () => {
    const source = makeMockSource("backbone.js");
    const backboneFrame = {
      ...makeMockFrame("3", source, undefined, 12, "updateEvents"),
      library: "backbone",
    };

    const { component } = render({ id: "3" }, backboneFrame);
    expect(component).toMatchSnapshot();
  });

  it("filename only", () => {
    const source = makeMockSource(
      "https://firefox.com/assets/src/js/foo-view.js"
    );
    const frame = makeMockFrame("1", source, undefined, 10, "renderFoo");

    const props = frameProperties(frame, null);
    const component = mount(<Frame {...props} />);
    expect(component.text()).toBe("    renderFoo foo-view.js:10");
  });

  it("full URL", () => {
    const url = `https://${"a".repeat(100)}.com/assets/src/js/foo-view.js`;
    const source = makeMockSource(url);
    const frame = makeMockFrame("1", source, undefined, 10, "renderFoo");

    const props = frameProperties(frame, null, { displayFullUrl: true });
    const component = mount(<Frame {...props} />);
    expect(component.text()).toBe(`    renderFoo ${url}:10`);
  });

  it("getFrameTitle", () => {
    const url = `https://${"a".repeat(100)}.com/assets/src/js/foo-view.js`;
    const source = makeMockSource(url);
    const frame = makeMockFrame("1", source, undefined, 10, "renderFoo");

    const props = frameProperties(frame, null, {
      getFrameTitle: x => `Jump to ${x}`,
    });
    const component = shallow(<Frame {...props} />);
    expect(component.prop("title")).toBe(`Jump to ${url}:10`);
    expect(component).toMatchSnapshot();
  });

  describe("mouse events", () => {
    it("does not call FrameMenu when disableContextMenu is true", () => {
      const { component } = render(undefined, undefined, {
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
        props.frame,
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
