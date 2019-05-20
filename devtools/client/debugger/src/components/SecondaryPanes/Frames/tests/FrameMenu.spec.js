/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import FrameMenu from "../FrameMenu";
import { kebabCase } from "lodash";

import { showMenu } from "devtools-contextmenu";
import { copyToTheClipboard } from "../../../../utils/clipboard";
import { makeMockFrame, makeMockSource } from "../../../../utils/test-mockup";

jest.mock("devtools-contextmenu", () => ({ showMenu: jest.fn() }));
jest.mock("../../../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn(),
}));

function generateMockId(labelString) {
  const label = L10N.getStr(labelString);
  return `node-menu-${kebabCase(label)}`;
}

describe("FrameMenu", () => {
  let mockEvent: any;
  let mockFrame;
  let emptyFrame: any;
  let callbacks;
  let frameworkGroupingOn;
  let toggleFrameworkGrouping;

  beforeEach(() => {
    mockFrame = makeMockFrame(undefined, makeMockSource("isFake"));
    mockEvent = {
      stopPropagation: jest.fn(),
      preventDefault: jest.fn(),
    };
    callbacks = {
      toggleFrameworkGrouping,
      toggleBlackbox: jest.fn(),
      copyToTheClipboard,
    };
    emptyFrame = {};
  });

  afterEach(() => {
    showMenu.mockClear();
  });

  it("sends three element in menuOpts to showMenu if source is present", () => {
    const sourceId = generateMockId("copySourceUri2");
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGroupingId = generateMockId("framework.enableGrouping");
    const blackBoxId = generateMockId("sourceFooter.blackbox");

    FrameMenu(mockFrame, frameworkGroupingOn, callbacks, mockEvent);

    const receivedArray = showMenu.mock.calls[0][1];
    expect(showMenu).toHaveBeenCalledWith(mockEvent, receivedArray);
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([
      frameworkGroupingId,
      sourceId,
      blackBoxId,
      stacktraceId,
    ]);
  });

  it("sends one element in menuOpts without source", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.enableGrouping");

    FrameMenu(emptyFrame, frameworkGroupingOn, callbacks, mockEvent);

    const receivedArray = showMenu.mock.calls[0][1];
    expect(showMenu).toHaveBeenCalledWith(mockEvent, receivedArray);
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });

  it("uses the disableGrouping text if frameworkGroupingOn is false", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.disableGrouping");

    FrameMenu(emptyFrame, true, callbacks, mockEvent);

    const receivedArray = showMenu.mock.calls[0][1];
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });

  it("uses the enableGrouping text if frameworkGroupingOn is true", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.enableGrouping");

    FrameMenu(emptyFrame, false, callbacks, mockEvent);

    const receivedArray = showMenu.mock.calls[0][1];
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });
});
