/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { mount, shallow } from "enzyme";

import Popover from "../Popover";

describe("Popover", () => {
  const onMouseLeave = jest.fn();
  const onKeyDown = jest.fn();
  const editorRef: any = {
    getBoundingClientRect() {
      return {
        x: 0,
        y: 0,
        width: 100,
        height: 100,
        top: 250,
        right: 0,
        bottom: 0,
        left: 20,
      };
    },
  };
  const targetPosition = {
    x: 100,
    y: 200,
    width: 300,
    height: 300,
    top: 50,
    right: 0,
    bottom: 0,
    left: 200,
  };
  const popover = mount(
    <Popover
      onMouseLeave={onMouseLeave}
      onKeyDown={onKeyDown}
      editorRef={editorRef}
      targetPosition={targetPosition}
    >
      <h1>Poppy!</h1>
    </Popover>
  );

  const tooltip = shallow(
    <Popover
      type="tooltip"
      onMouseLeave={onMouseLeave}
      onKeyDown={onKeyDown}
      editorRef={editorRef}
      targetPosition={targetPosition}
    >
      <h1>Toolie!</h1>
    </Popover>
  );

  const div = document.createElement("div");

  const event = { currentTarget: div };

  beforeEach(() => {
    onMouseLeave.mockClear();
    onKeyDown.mockClear();
  });

  it("render", () => expect(popover).toMatchSnapshot());

  it("render (tooltip)", () => expect(tooltip).toMatchSnapshot());

  it("calls mouseLeave", () => {
    popover.find(".popover").simulate("mouseleave", event);
    expect(onMouseLeave).toHaveBeenCalled();
  });

  it("calls mouseLeave (tooltip)", () => {
    tooltip.find(".tooltip").simulate("mouseleave", event);
    expect(onMouseLeave).toHaveBeenCalled();
  });

  it("no mouse leave on bracket or gap", () => {
    popover.find(".bracket-arrow").simulate("mouseleave", event);
    expect(onMouseLeave).not.toHaveBeenCalled();
  });

  it("calls keyDown", () => {
    popover.find(".popover").simulate("keydown", { key: "Escape" });
    expect(onKeyDown).toHaveBeenCalled();
  });

  it("calls keyDown (tooltip)", () => {
    tooltip.find(".tooltip").simulate("keydown", { key: "Escape" });
    expect(onKeyDown).toHaveBeenCalled();
  });

  it("mount popover", () => {
    const mountedPopover = mount(
      <Popover
        onMouseLeave={onMouseLeave}
        onKeyDown={onKeyDown}
        editorRef={editorRef}
        targetPosition={targetPosition}
      >
        <h1>Poppy!</h1>
      </Popover>
    );
    expect(mountedPopover).toMatchSnapshot();
  });

  it("mount tooltip", () => {
    const mountedTooltip = mount(
      <Popover
        type="tooltip"
        onMouseLeave={onMouseLeave}
        onKeyDown={onKeyDown}
        editorRef={editorRef}
        targetPosition={targetPosition}
      >
        <h1>Toolie!</h1>
      </Popover>
    );
    expect(mountedTooltip).toMatchSnapshot();
  });

  it("tooltip normally displays above the target", () => {
    const editor: any = {
      getBoundingClientRect() {
        return {
          width: 500,
          height: 500,
          top: 0,
          bottom: 500,
          left: 0,
          right: 500,
        };
      },
    };
    const target = {
      width: 30,
      height: 10,
      top: 100,
      bottom: 110,
      left: 20,
      right: 50,
    };

    const mountedTooltip = mount(
      <Popover
        type="tooltip"
        onMouseLeave={onMouseLeave}
        onKeyDown={onKeyDown}
        editorRef={editor}
        targetPosition={target}
      >
        <h1>Toolie!</h1>
      </Popover>
    );

    const toolTipTop = parseInt(mountedTooltip.getDOMNode().style.top, 10);
    expect(toolTipTop).toBeLessThanOrEqual(target.top);
  });

  it("tooltop won't display above the target when insufficient space", () => {
    const editor: any = {
      getBoundingClientRect() {
        return {
          width: 100,
          height: 100,
          top: 0,
          bottom: 100,
          left: 0,
          right: 100,
        };
      },
    };
    const target = {
      width: 30,
      height: 10,
      top: 0,
      bottom: 10,
      left: 20,
      right: 50,
    };

    const mountedTooltip = mount(
      <Popover
        type="tooltip"
        onMouseLeave={onMouseLeave}
        onKeyDown={onKeyDown}
        editorRef={editor}
        targetPosition={target}
      >
        <h1>Toolie!</h1>
      </Popover>
    );

    const toolTipTop = parseInt(mountedTooltip.getDOMNode().style.top, 10);
    expect(toolTipTop).toBeGreaterThanOrEqual(target.bottom);
  });
});
