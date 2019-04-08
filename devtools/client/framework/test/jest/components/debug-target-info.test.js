/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the DebugTargetInfo component.
 */

const renderer = require("react-test-renderer");
const React = require("devtools/client/shared/vendor/react");
const DebugTargetInfo = React.createFactory(require("devtools/client/framework/components/DebugTargetInfo"));
const { CONNECTION_TYPES, DEBUG_TARGET_TYPES } =
  require("devtools/client/shared/remote-debugging/constants");

/**
 * Stub for the L10N property expected by the DebugTargetInfo component.
 */
const stubL10N = {
  getStr: id => id,
  getFormatStr: (id, ...args) => [id, ...args].join("-"),
};

const findByClassName = (testInstance, className) => {
  return testInstance.findAll(node => {
    return node.props.className && node.props.className.includes(className);
  });
};

const TEST_TOOLBOX = {
  target: {
    name: "Test Tab Name",
    url: "http://some.target/url",
  },
};

const TEST_TOOLBOX_NO_NAME = {
  target: {
    url: "http://some.target/without/a/name",
  },
};

const USB_DEVICE_DESCRIPTION = {
  brandName: "usbRuntimeBrandName",
  connectionType: CONNECTION_TYPES.USB,
  channel: "release",
  deviceName: "usbDeviceName",
  version: "1.0.0",
};

const THIS_FIREFOX_DEVICE_DESCRIPTION = {
  brandName: "thisFirefoxRuntimeBrandName",
  connectionType: CONNECTION_TYPES.THIS_FIREFOX,
  channel: "release",
  version: "1.0.0",
};

const USB_TARGET_INFO = DebugTargetInfo({
  deviceDescription: USB_DEVICE_DESCRIPTION,
  targetType: DEBUG_TARGET_TYPES.TAB,
  toolbox: TEST_TOOLBOX,
  L10N: stubL10N,
});

const THIS_FIREFOX_TARGET_INFO = DebugTargetInfo({
  deviceDescription: THIS_FIREFOX_DEVICE_DESCRIPTION,
  targetType: DEBUG_TARGET_TYPES.TAB,
  toolbox: TEST_TOOLBOX,
  L10N: stubL10N,
});

const THIS_FIREFOX_NO_NAME_TARGET_INFO = DebugTargetInfo({
  deviceDescription: THIS_FIREFOX_DEVICE_DESCRIPTION,
  targetType: DEBUG_TARGET_TYPES.TAB,
  toolbox: TEST_TOOLBOX_NO_NAME,
  L10N: stubL10N,
});

describe("DebugTargetInfo component", () => {
  it("displays connection info for USB Release target", () => {
    const targetInfo = renderer.create(USB_TARGET_INFO);
    expect(findByClassName(targetInfo.root, "js-connection-info").length).toEqual(1);
  });

  it("renders the expected snapshot for USB Release target", () => {
    const targetInfo = renderer.create(USB_TARGET_INFO);
    expect(targetInfo.toJSON()).toMatchSnapshot();
  });

  it("hides the connection info for This Firefox target", () => {
    const targetInfo = renderer.create(THIS_FIREFOX_TARGET_INFO);
    expect(findByClassName(targetInfo.root, "js-connection-info").length).toEqual(0);
  });

  it("displays the target title if the target of the Toolbox has a name", () => {
    const targetInfo = renderer.create(THIS_FIREFOX_TARGET_INFO);
    expect(findByClassName(targetInfo.root, "js-target-title").length).toEqual(1);
  });

  it("renders the expected snapshot for This Firefox target", () => {
    const targetInfo = renderer.create(THIS_FIREFOX_TARGET_INFO);
    expect(targetInfo.toJSON()).toMatchSnapshot();
  });

  it("doesn't display the target title if the target of the Toolbox has no name", () => {
    const targetInfo = renderer.create(THIS_FIREFOX_NO_NAME_TARGET_INFO);
    expect(findByClassName(targetInfo.root, "js-target-title").length).toEqual(0);
  });

  it("renders the expected snapshot for a Toolbox with an unnamed target", () => {
    const targetInfo = renderer.create(THIS_FIREFOX_NO_NAME_TARGET_INFO);
    expect(targetInfo.toJSON()).toMatchSnapshot();
  });
});
