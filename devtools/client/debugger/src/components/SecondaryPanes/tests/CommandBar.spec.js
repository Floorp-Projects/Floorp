/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import CommandBar from "../CommandBar";
import { mockthreadcx } from "../../../utils/test-mockup";

describe("CommandBar", () => {
  it("f8 key command calls props.breakOnNext when not in paused state", () => {
    const props = {
      cx: mockthreadcx,
      breakOnNext: jest.fn(),
      resume: jest.fn(),
      isPaused: false,
    };
    const mockEvent = {
      preventDefault: jest.fn(),
      stopPropagation: jest.fn(),
    };

    // The "on" spy will see all the keyboard listeners being registered by
    // the shortcuts.on function
    const context = { shortcuts: { on: jest.fn() } };

    // $FlowIgnore
    shallow(<CommandBar.WrappedComponent {...props} />, { context });

    // get the keyboard event listeners recorded from the "on" spy.
    // this will be an array where each item is itself a two item array
    // containing the key code and the corresponding handler for that key code
    const keyEventHandlers = context.shortcuts.on.mock.calls;

    // simulate pressing the F8 key by calling the F8 handlers
    keyEventHandlers
      .filter(i => i[0] === "F8")
      .forEach(([_, handler]) => {
        handler(null, mockEvent);
      });

    expect(props.breakOnNext).toHaveBeenCalled();
    expect(props.resume).not.toHaveBeenCalled();
  });

  it("f8 key command calls props.resume when in paused state", () => {
    const props = {
      cx: { ...mockthreadcx, isPaused: true },
      breakOnNext: jest.fn(),
      resume: jest.fn(),
      isPaused: true,
    };
    const mockEvent = {
      preventDefault: jest.fn(),
      stopPropagation: jest.fn(),
    };

    // The "on" spy will see all the keyboard listeners being registered by
    // the shortcuts.on function
    const context = { shortcuts: { on: jest.fn() } };

    // $FlowIgnore
    shallow(<CommandBar.WrappedComponent {...props} />, { context });

    // get the keyboard event listeners recorded from the "on" spy.
    // this will be an array where each item is itself a two item array
    // containing the key code and the corresponding handler for that key code
    const keyEventHandlers = context.shortcuts.on.mock.calls;

    // simulate pressing the F8 key by calling the F8 handlers
    keyEventHandlers
      .filter(i => i[0] === "F8")
      .forEach(([_, handler]) => {
        handler(null, mockEvent);
      });
    expect(props.resume).toHaveBeenCalled();
    expect(props.breakOnNext).not.toHaveBeenCalled();
  });
});
