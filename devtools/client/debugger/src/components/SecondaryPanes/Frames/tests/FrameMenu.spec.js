/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import FrameMenu from "../FrameMenu";
import { kebabCase } from "lodash";

import { showMenu } from "../../../../context-menu/menu";
import { copyToTheClipboard } from "../../../../utils/clipboard";
import {
  makeMockFrame,
  makeMockSource,
  mockthreadcx,
} from "../../../../utils/test-mockup";

jest.mock("../../../../context-menu/menu", () => ({ showMenu: jest.fn() }));
jest.mock("../../../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn(),
}));

function generateMockId(labelString) {
  const label = L10N.getStr(labelString);
  return `node-menu-${kebabCase(label)}`;
}

describe("FrameMenu", () => {
  let mockEvent;
  let mockFrame;
  let emptyFrame;
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
      restart: jest.fn(),
    };
    emptyFrame = {};
  });

  afterEach(() => {
    showMenu.mockClear();
  });

  it("sends three element in menuOpts to showMenu if source is present", () => {
    const restartFrameId = generateMockId("restartFrame");
    const sourceId = generateMockId("copySourceUri2");
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGroupingId = generateMockId("framework.enableGrouping");
    const blackBoxId = generateMockId("sourceFooter.ignore");

    FrameMenu(
      mockFrame,
      frameworkGroupingOn,
      callbacks,
      mockEvent,
      mockthreadcx
    );

    const receivedArray = showMenu.mock.calls[0][1];
    expect(showMenu).toHaveBeenCalledWith(mockEvent, receivedArray);
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([
      restartFrameId,
      frameworkGroupingId,
      sourceId,
      blackBoxId,
      stacktraceId,
    ]);
  });

  it("sends one element in menuOpts without source", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.enableGrouping");

    FrameMenu(
      emptyFrame,
      frameworkGroupingOn,
      callbacks,
      mockEvent,
      mockthreadcx
    );

    const receivedArray = showMenu.mock.calls[0][1];
    expect(showMenu).toHaveBeenCalledWith(mockEvent, receivedArray);
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });

  it("uses the disableGrouping text if frameworkGroupingOn is false", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.disableGrouping");

    FrameMenu(emptyFrame, true, callbacks, mockEvent, mockthreadcx);

    const receivedArray = showMenu.mock.calls[0][1];
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });

  it("uses the enableGrouping text if frameworkGroupingOn is true", () => {
    const stacktraceId = generateMockId("copyStackTrace");
    const frameworkGrouping = generateMockId("framework.enableGrouping");

    FrameMenu(emptyFrame, false, callbacks, mockEvent, mockthreadcx);

    const receivedArray = showMenu.mock.calls[0][1];
    const receivedArrayIds = receivedArray.map(item => item.id);
    expect(receivedArrayIds).toEqual([frameworkGrouping, stacktraceId]);
  });
});
