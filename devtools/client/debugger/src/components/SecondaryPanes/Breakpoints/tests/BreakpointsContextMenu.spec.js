/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import BreakpointsContextMenu from "../BreakpointsContextMenu";
import { buildMenu } from "devtools-contextmenu";

import {
  makeMockBreakpoint,
  makeMockSource,
  mockcx,
} from "../../../../utils/test-mockup";

jest.mock("devtools-contextmenu");

function render(disabled = false) {
  const props = generateDefaults(disabled);
  const component = shallow(<BreakpointsContextMenu {...props} />);
  return { component, props };
}

function generateDefaults(disabled) {
  const source = makeMockSource(
    "https://example.com/main.js",
    "source-https://example.com/main.js"
  );
  const breakpoints = [
    {
      ...makeMockBreakpoint(source, 1),
      id: "https://example.com/main.js:1:",
      disabled: disabled,
      options: {
        condition: "",
        logValue: "",
        hidden: false,
      },
    },
    {
      ...makeMockBreakpoint(source, 2),
      id: "https://example.com/main.js:2:",
      disabled: disabled,
      options: {
        hidden: false,
      },
    },
    {
      ...makeMockBreakpoint(source, 3),
      id: "https://example.com/main.js:3:",
      disabled: disabled,
    },
  ];

  const props = {
    cx: mockcx,
    breakpoints,
    breakpoint: breakpoints[0],
    removeBreakpoint: jest.fn(),
    removeBreakpoints: jest.fn(),
    removeAllBreakpoints: jest.fn(),
    toggleBreakpoints: jest.fn(),
    toggleAllBreakpoints: jest.fn(),
    toggleDisabledBreakpoint: jest.fn(),
    selectSpecificLocation: jest.fn(),
    setBreakpointCondition: jest.fn(),
    openConditionalPanel: jest.fn(),
    contextMenuEvent: ({ preventDefault: jest.fn() }: any),
    selectedSource: makeMockSource(),
    setBreakpointOptions: jest.fn(),
  };
  return props;
}

describe("BreakpointsContextMenu", () => {
  afterEach(() => {
    buildMenu.mockReset();
  });

  describe("context menu actions affecting other breakpoints", () => {
    it("'remove others' calls removeBreakpoints with proper arguments", () => {
      const { props } = render();
      const menuItems = buildMenu.mock.calls[0][0];
      const deleteOthers = menuItems.find(
        item => item.item.id === "node-menu-delete-other"
      );
      deleteOthers.item.click();

      expect(props.removeBreakpoints).toHaveBeenCalled();

      const otherBreakpoints = [props.breakpoints[1], props.breakpoints[2]];
      expect(props.removeBreakpoints.mock.calls[0][1]).toEqual(
        otherBreakpoints
      );
    });

    it("'enable others' calls toggleBreakpoints with proper arguments", () => {
      const { props } = render(true);
      const menuItems = buildMenu.mock.calls[0][0];
      const enableOthers = menuItems.find(
        item => item.item.id === "node-menu-enable-others"
      );
      enableOthers.item.click();

      expect(props.toggleBreakpoints).toHaveBeenCalled();

      expect(props.toggleBreakpoints.mock.calls[0][1]).toBe(false);

      const otherBreakpoints = [props.breakpoints[1], props.breakpoints[2]];
      expect(props.toggleBreakpoints.mock.calls[0][2]).toEqual(
        otherBreakpoints
      );
    });

    it("'disable others' calls toggleBreakpoints with proper arguments", () => {
      const { props } = render();
      const menuItems = buildMenu.mock.calls[0][0];
      const disableOthers = menuItems.find(
        item => item.item.id === "node-menu-disable-others"
      );
      disableOthers.item.click();

      expect(props.toggleBreakpoints).toHaveBeenCalled();
      expect(props.toggleBreakpoints.mock.calls[0][1]).toBe(true);

      const otherBreakpoints = [props.breakpoints[1], props.breakpoints[2]];
      expect(props.toggleBreakpoints.mock.calls[0][2]).toEqual(
        otherBreakpoints
      );
    });
  });
});
