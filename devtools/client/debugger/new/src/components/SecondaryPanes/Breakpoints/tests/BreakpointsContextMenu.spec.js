/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import BreakpointsContextMenu from "../BreakpointsContextMenu";
import { createBreakpoint } from "../../../../utils/breakpoint";
import { buildMenu } from "devtools-contextmenu";

jest.mock("devtools-contextmenu");

function render(overrides = {}, disabled = false) {
  const props = generateDefaults(overrides, disabled);
  const component = shallow(<BreakpointsContextMenu {...props} />);
  return { component, props };
}

function generateDefaults(overrides = {}, disabled) {
  const breakpoints = [
    createBreakpoint(
      {
        line: 1,
        column: undefined,
        sourceId: "server1.conn26.child3/source26",
        sourceUrl: "https://example.com/main.js"
      },
      { id: "https://example.com/main.js:1:", disabled: disabled }
    ),
    createBreakpoint(
      {
        line: 2,
        column: undefined,
        sourceId: "server1.conn26.child3/source26",
        sourceUrl: "https://example.com/main.js"
      },
      { id: "https://example.com/main.js:2:", disabled: disabled }
    ),
    createBreakpoint(
      {
        line: 3,
        column: undefined,
        sourceId: "server1.conn26.child3/source26",
        sourceUrl: "https://example.com/main.js"
      },
      { id: "https://example.com/main.js:3:", disabled: disabled }
    )
  ];

  const props = {
    breakpoints,
    breakpoint: { id: "https://example.com/main.js:1:" },
    removeBreakpoint: jest.fn(),
    removeBreakpoints: jest.fn(),
    removeAllBreakpoints: jest.fn(),
    toggleBreakpoints: jest.fn(),
    toggleAllBreakpoints: jest.fn(),
    toggleDisabledBreakpoint: jest.fn(),
    selectSpecificLocation: jest.fn(),
    setBreakpointCondition: jest.fn(),
    openConditionalPanel: jest.fn(),
    contextMenuEvent: { preventDefault: jest.fn() },
    ...overrides
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
      expect(props.removeBreakpoints.mock.calls[0][0]).toEqual(
        otherBreakpoints
      );
    });

    it("'enable others' calls toggleBreakpoints with proper arguments", () => {
      const { props } = render({}, true);
      const menuItems = buildMenu.mock.calls[0][0];
      const enableOthers = menuItems.find(
        item => item.item.id === "node-menu-enable-others"
      );
      enableOthers.item.click();

      expect(props.toggleBreakpoints).toHaveBeenCalled();

      expect(props.toggleBreakpoints.mock.calls[0][0]).toBe(false);

      const otherBreakpoints = [props.breakpoints[1], props.breakpoints[2]];
      expect(props.toggleBreakpoints.mock.calls[0][1]).toEqual(
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
      expect(props.toggleBreakpoints.mock.calls[0][0]).toBe(true);

      const otherBreakpoints = [props.breakpoints[1], props.breakpoints[2]];
      expect(props.toggleBreakpoints.mock.calls[0][1]).toEqual(
        otherBreakpoints
      );
    });
  });
});
