/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { mount, shallow } from "enzyme";
import Frames from "../index.js";
// eslint-disable-next-line
import { formatCallStackFrames } from "../../../../selectors/getCallStackFrames";
import { makeMockFrame, makeMockSource } from "../../../../utils/test-mockup";
import { createInitial, insertResources } from "../../../../utils/resource";
import type { SourceResourceState } from "../../../../reducers/sources";

function render(overrides = {}) {
  const defaultProps = {
    frames: null,
    selectedFrame: null,
    frameworkGroupingOn: false,
    toggleFrameworkGrouping: jest.fn(),
    contextTypes: {},
    selectFrame: jest.fn(),
    toggleBlackBox: jest.fn(),
  };

  const props = { ...defaultProps, ...overrides };
  // $FlowIgnore
  const component = shallow(<Frames.WrappedComponent {...props} />, {
    context: { l10n: L10N },
  });

  return component;
}

describe("Frames", () => {
  describe("Supports different number of frames", () => {
    it("empty frames", () => {
      const component = render();
      expect(component).toMatchSnapshot();
      expect(component.find(".show-more").exists()).toBeFalsy();
    });

    it("one frame", () => {
      const frames = [{ id: 1 }];
      const selectedFrame = frames[0];
      const component = render({ frames, selectedFrame });

      expect(component.find(".show-more").exists()).toBeFalsy();
      expect(component).toMatchSnapshot();
    });

    it("toggling the show more button", () => {
      const frames = [
        { id: 1 },
        { id: 2 },
        { id: 3 },
        { id: 4 },
        { id: 5 },
        { id: 6 },
        { id: 7 },
        { id: 8 },
        { id: 9 },
        { id: 10 },
      ];

      const selectedFrame = frames[0];
      const component = render({ selectedFrame, frames });

      const getToggleBtn = () => component.find(".show-more");
      const getFrames = () => component.find("Frame");

      expect(getToggleBtn().text()).toEqual("Expand rows");
      expect(getFrames()).toHaveLength(7);

      getToggleBtn().simulate("click");
      expect(getToggleBtn().text()).toEqual("Collapse rows");
      expect(getFrames()).toHaveLength(10);
      expect(component).toMatchSnapshot();
    });

    it("disable frame truncation", () => {
      const framesNumber = 20;
      const frames = Array.from({ length: framesNumber }, (_, i) => ({
        id: i + 1,
      }));

      const component = render({
        frames,
        disableFrameTruncate: true,
      });

      const getToggleBtn = () => component.find(".show-more");
      const getFrames = () => component.find("Frame");

      expect(getToggleBtn().exists()).toBeFalsy();
      expect(getFrames()).toHaveLength(framesNumber);

      expect(component).toMatchSnapshot();
    });

    it("shows the full URL", () => {
      const frames = [
        {
          id: 1,
          displayName: "renderFoo",
          location: {
            line: 55,
          },
          source: {
            url: "http://myfile.com/mahscripts.js",
          },
        },
      ];

      const component = mount(
        // $FlowIgnore
        <Frames.WrappedComponent
          frames={frames}
          disableFrameTruncate={true}
          displayFullUrl={true}
        />
      );
      expect(component.text()).toBe(
        "renderFoo http://myfile.com/mahscripts.js:55"
      );
    });

    it("passes the getFrameTitle prop to the Frame component", () => {
      const frames = [
        {
          id: 1,
          displayName: "renderFoo",
          location: {
            line: 55,
          },
          source: {
            url: "http://myfile.com/mahscripts.js",
          },
        },
      ];
      const getFrameTitle = () => {};
      const component = render({ frames, getFrameTitle });

      expect(component.find("Frame").prop("getFrameTitle")).toBe(getFrameTitle);
      expect(component).toMatchSnapshot();
    });

    it("passes the getFrameTitle prop to the Group component", () => {
      const frames = [
        {
          id: 1,
          displayName: "renderFoo",
          location: {
            line: 55,
          },
          source: {
            url: "http://myfile.com/mahscripts.js",
          },
        },
        {
          id: 2,
          library: "back",
          displayName: "a",
          location: {
            line: 55,
          },
          source: {
            url: "http://myfile.com/back.js",
          },
        },
        {
          id: 3,
          library: "back",
          displayName: "b",
          location: {
            line: 55,
          },
          source: {
            url: "http://myfile.com/back.js",
          },
        },
      ];
      const getFrameTitle = () => {};
      const component = render({
        frames,
        getFrameTitle,
        frameworkGroupingOn: true,
      });

      expect(component.find("Group").prop("getFrameTitle")).toBe(getFrameTitle);
    });
  });

  describe("Blackboxed Frames", () => {
    it("filters blackboxed frames", () => {
      const source1 = makeMockSource(undefined, "1");
      const source2 = makeMockSource(undefined, "2");
      (source2: any).isBlackBoxed = true;

      const frames = [
        makeMockFrame("1", source1),
        makeMockFrame("2", source2),
        makeMockFrame("3", source1),
        makeMockFrame("8", source2),
      ];

      const sources: SourceResourceState = insertResources(createInitial(), [
        { ...source1, content: null },
        { ...source2, content: null },
      ]);

      const processedFrames = formatCallStackFrames(frames, sources, source1);
      const selectedFrame = frames[0];

      const component = render({
        frames: processedFrames,
        frameworkGroupingOn: false,
        selectedFrame,
      });

      expect(component.find("Frame")).toHaveLength(2);
      expect(component).toMatchSnapshot();
    });
  });

  describe("Library Frames", () => {
    it("toggling framework frames", () => {
      const frames = [
        { id: 1 },
        { id: 2, library: "back" },
        { id: 3, library: "back" },
        { id: 8 },
      ];

      const selectedFrame = frames[0];
      const frameworkGroupingOn = false;
      const component = render({ frames, frameworkGroupingOn, selectedFrame });

      expect(component.find("Frame")).toHaveLength(4);
      expect(component).toMatchSnapshot();

      component.setProps({ frameworkGroupingOn: true });

      expect(component.find("Frame")).toHaveLength(2);
      expect(component).toMatchSnapshot();
    });

    it("groups all the Webpack-related frames", () => {
      const frames = [
        { id: "1-appFrame" },
        {
          id: "2-webpackBootstrapFrame",
          source: { url: "webpack:///webpack/bootstrap 01d88449ca6e9335a66f" },
        },
        {
          id: "3-webpackBundleFrame",
          source: { url: "https://foo.com/bundle.js" },
        },
        {
          id: "4-webpackBootstrapFrame",
          source: { url: "webpack:///webpack/bootstrap 01d88449ca6e9335a66f" },
        },
        {
          id: "5-webpackBundleFrame",
          source: { url: "https://foo.com/bundle.js" },
        },
      ];
      const selectedFrame = frames[0];
      const frameworkGroupingOn = true;
      const component = render({ frames, frameworkGroupingOn, selectedFrame });

      expect(component).toMatchSnapshot();
    });

    it("selectable framework frames", () => {
      const frames = [
        { id: 1 },
        { id: 2, library: "back" },
        { id: 3, library: "back" },
        { id: 8 },
      ];

      const selectedFrame = frames[0];

      const component = render({
        frames,
        frameworkGroupingOn: false,
        selectedFrame,
        selectable: true,
      });
      expect(component).toMatchSnapshot();

      component.setProps({ frameworkGroupingOn: true });
      expect(component).toMatchSnapshot();
    });
  });
});
