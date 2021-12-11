/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the DebugTargetInfo component.
 */

const renderer = require("react-test-renderer");
const React = require("devtools/client/shared/vendor/react");
const DebugTargetInfo = React.createFactory(
  require("devtools/client/framework/components/DebugTargetInfo")
);
const {
  CONNECTION_TYPES,
  DEBUG_TARGET_TYPES,
} = require("devtools/client/shared/remote-debugging/constants");

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
    targetForm: {
      traits: {
        navigation: true,
      },
    },
    getTrait: trait => {
      return TEST_TOOLBOX.target.targetForm.traits[trait];
    },
  },
  doc: {},
};

const TEST_TOOLBOX_NO_NAME = {
  target: {
    url: "http://some.target/without/a/name",
    targetForm: {
      traits: {
        navigation: true,
      },
    },
    getTrait: trait => {
      return TEST_TOOLBOX.target.targetForm.traits[trait];
    },
  },
  doc: {},
};

const USB_DEVICE_DESCRIPTION = {
  deviceName: "usbDeviceName",
  icon: "chrome://devtools/skin/images/aboutdebugging-firefox-release.svg",
  name: "usbRuntimeBrandName",
  version: "1.0.0",
};

const THIS_FIREFOX_DEVICE_DESCRIPTION = {
  icon: "chrome://devtools/skin/images/aboutdebugging-firefox-release.svg",
  version: "1.0.0",
  name: "thisFirefoxRuntimeBrandName",
};

const USB_TARGET_INFO = {
  debugTargetData: {
    connectionType: CONNECTION_TYPES.USB,
    runtimeInfo: USB_DEVICE_DESCRIPTION,
    targetType: DEBUG_TARGET_TYPES.TAB,
  },
  toolbox: TEST_TOOLBOX,
  L10N: stubL10N,
};

const THIS_FIREFOX_TARGET_INFO = {
  debugTargetData: {
    connectionType: CONNECTION_TYPES.THIS_FIREFOX,
    runtimeInfo: THIS_FIREFOX_DEVICE_DESCRIPTION,
    targetType: DEBUG_TARGET_TYPES.TAB,
  },
  toolbox: TEST_TOOLBOX,
  L10N: stubL10N,
};

const THIS_FIREFOX_NO_NAME_TARGET_INFO = {
  debugTargetData: {
    connectionType: CONNECTION_TYPES.THIS_FIREFOX,
    runtimeInfo: THIS_FIREFOX_DEVICE_DESCRIPTION,
    targetType: DEBUG_TARGET_TYPES.TAB,
  },
  toolbox: TEST_TOOLBOX_NO_NAME,
  L10N: stubL10N,
};

describe("DebugTargetInfo component", () => {
  describe("Connection info", () => {
    it("displays connection info for USB Release target", () => {
      const component = renderer.create(DebugTargetInfo(USB_TARGET_INFO));
      expect(
        findByClassName(component.root, "qa-connection-info").length
      ).toEqual(1);
    });

    it("renders the expected snapshot for USB Release target", () => {
      const component = renderer.create(DebugTargetInfo(USB_TARGET_INFO));
      expect(component.toJSON()).toMatchSnapshot();
    });

    it("hides the connection info for This Firefox target", () => {
      const component = renderer.create(
        DebugTargetInfo(THIS_FIREFOX_TARGET_INFO)
      );
      expect(
        findByClassName(component.root, "qa-connection-info").length
      ).toEqual(0);
    });
  });

  describe("Target title", () => {
    it("displays the target title if the target of the Toolbox has a name", () => {
      const component = renderer.create(
        DebugTargetInfo(THIS_FIREFOX_TARGET_INFO)
      );
      expect(findByClassName(component.root, "qa-target-title").length).toEqual(
        1
      );
    });

    it("renders the expected snapshot for This Firefox target", () => {
      const component = renderer.create(
        DebugTargetInfo(THIS_FIREFOX_TARGET_INFO)
      );
      expect(component.toJSON()).toMatchSnapshot();
    });

    it("doesn't display the target title if the target of the Toolbox has no name", () => {
      const component = renderer.create(
        DebugTargetInfo(THIS_FIREFOX_NO_NAME_TARGET_INFO)
      );
      expect(findByClassName(component.root, "qa-target-title").length).toEqual(
        0
      );
    });

    it("renders the expected snapshot for a Toolbox with an unnamed target", () => {
      const component = renderer.create(
        DebugTargetInfo(THIS_FIREFOX_NO_NAME_TARGET_INFO)
      );
      expect(component.toJSON()).toMatchSnapshot();
    });
  });

  describe("Target icon", () => {
    const buildProps = (base, extraDebugTargetData) => {
      const props = Object.assign({}, base);
      Object.assign(props.debugTargetData, extraDebugTargetData);
      return props;
    };

    it("renders the expected snapshot for a tab target", () => {
      const props = buildProps(USB_TARGET_INFO, {
        targetType: DEBUG_TARGET_TYPES.TAB,
      });
      const component = renderer.create(DebugTargetInfo(props));
      expect(component.toJSON()).toMatchSnapshot();
    });

    it("renders the expected snapshot for a worker target", () => {
      const props = buildProps(USB_TARGET_INFO, {
        targetType: DEBUG_TARGET_TYPES.WORKER,
      });
      const component = renderer.create(DebugTargetInfo(props));
      expect(component.toJSON()).toMatchSnapshot();
    });

    it("renders the expected snapshot for an extension target", () => {
      const props = buildProps(USB_TARGET_INFO, {
        targetType: DEBUG_TARGET_TYPES.EXTENSION,
      });
      const component = renderer.create(DebugTargetInfo(props));
      expect(component.toJSON()).toMatchSnapshot();
    });

    it("renders the expected snapshot for a process target", () => {
      const props = buildProps(USB_TARGET_INFO, {
        targetType: DEBUG_TARGET_TYPES.PROCESS,
      });
      const component = renderer.create(DebugTargetInfo(props));
      expect(component.toJSON()).toMatchSnapshot();
    });
  });
});
