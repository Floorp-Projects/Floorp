/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { copyToTheClipboard } from "../clipboard";

let clipboardTestCopyString, expectedCopyEvent;
const addEventListener = jest.fn();
const execCommand = jest.fn();
const removeEventListener = jest.fn();

describe("copyToTheClipboard()", () => {
  beforeEach(() => {
    expectedCopyEvent = "copy";
    clipboardTestCopyString = "content intended for clipboard";

    global.document.addEventListener = addEventListener;
    global.document.execCommand = execCommand;
    global.document.removeEventListener = removeEventListener;
  });

  it("listens for 'copy' event", () => {
    copyToTheClipboard(clipboardTestCopyString);

    expect(document.addEventListener).toHaveBeenCalledWith(
      expectedCopyEvent,
      expect.anything()
    );
  });

  it("calls document.execCommand() with 'copy' command", () => {
    copyToTheClipboard(clipboardTestCopyString);

    expect(execCommand).toHaveBeenCalledWith(expectedCopyEvent, false, null);
  });

  it("removes event listener for 'copy' event", () => {
    copyToTheClipboard(clipboardTestCopyString);

    expect(document.removeEventListener).toHaveBeenCalledWith(
      expectedCopyEvent,
      expect.anything()
    );
  });
});
