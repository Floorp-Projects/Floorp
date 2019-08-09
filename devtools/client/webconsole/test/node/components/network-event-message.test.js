/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
const NetworkEventMessage = createFactory(
  require("devtools/client/webconsole/components/Output/message-types/NetworkEventMessage")
);
const {
  INDENT_WIDTH,
} = require("devtools/client/webconsole/components/Output/MessageIndent");

// Test fakes.
const {
  stubPreparedMessages,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/node/fixtures/serviceContainer");

const EXPECTED_URL = "http://example.com/inexistent.html";
const EXPECTED_STATUS = /\[HTTP\/\d\.\d \d+ [A-Za-z ]+ \d+ms\]/;

describe("NetworkEventMessage component:", () => {
  describe("GET request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("GET request");
      const update = stubPreparedMessages.get("GET request update");
      const wrapper = render(
        NetworkEventMessage({
          message,
          serviceContainer,
          timestampsVisible: true,
          networkMessageUpdate: update,
        })
      );
      const {
        timestampString,
      } = require("devtools/client/webconsole/utils/l10n");

      expect(wrapper.find(".timestamp").text()).toBe(
        timestampString(message.timeStamp)
      );
      expect(wrapper.find(".message-body .method").text()).toBe("GET");
      expect(wrapper.find(".message-body .xhr").length).toBe(0);
      expect(wrapper.find(".message-body .url").length).toBe(1);
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find(".message-body .status").length).toBe(1);
      expect(wrapper.find(".message-body .status").text()).toMatch(
        EXPECTED_STATUS
      );
    });

    it("does not have a timestamp when timestampsVisible prop is falsy", () => {
      const message = stubPreparedMessages.get("GET request update");
      const wrapper = render(
        NetworkEventMessage({
          message,
          serviceContainer,
          timestampsVisible: false,
        })
      );

      expect(wrapper.find(".timestamp").length).toBe(0);
    });

    it("has the expected indent", () => {
      const message = stubPreparedMessages.get("GET request");

      const indent = 10;
      let wrapper = render(
        NetworkEventMessage({
          message: Object.assign({}, message, { indent }),
          serviceContainer,
        })
      );
      let indentEl = wrapper.find(".indent");
      expect(indentEl.prop("style").width).toBe(`${indent * INDENT_WIDTH}px`);
      expect(indentEl.prop("data-indent")).toBe(`${indent}`);

      wrapper = render(NetworkEventMessage({ message, serviceContainer }));
      indentEl = wrapper.find(".indent");
      expect(indentEl.prop("style").width).toBe(`0`);
      expect(indentEl.prop("data-indent")).toBe(`0`);
    });
  });

  describe("XHR GET request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("XHR GET request");
      const update = stubPreparedMessages.get("XHR GET request update");
      const wrapper = render(
        NetworkEventMessage({
          message,
          serviceContainer,
          networkMessageUpdate: update,
        })
      );

      expect(wrapper.find(".message-body .method").text()).toBe("GET");
      expect(wrapper.find(".message-body .xhr").length).toBe(1);
      expect(wrapper.find(".message-body .xhr").text()).toBe("XHR");
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find(".message-body .status").text()).toMatch(
        EXPECTED_STATUS
      );
    });
  });

  describe("XHR POST request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("XHR POST request");
      const update = stubPreparedMessages.get("XHR POST request update");
      const wrapper = render(
        NetworkEventMessage({
          message,
          serviceContainer,
          networkMessageUpdate: update,
        })
      );

      expect(wrapper.find(".message-body .method").text()).toBe("POST");
      expect(wrapper.find(".message-body .xhr").length).toBe(1);
      expect(wrapper.find(".message-body .xhr").text()).toBe("XHR");
      expect(wrapper.find(".message-body .url").length).toBe(1);
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find(".message-body .status").length).toBe(1);
      expect(wrapper.find(".message-body .status").text()).toMatch(
        EXPECTED_STATUS
      );
    });
  });

  describe("is expandable", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("XHR POST request");
      const wrapper = render(
        NetworkEventMessage({
          message,
          serviceContainer,
        })
      );

      expect(wrapper.find(".message .theme-twisty")).toExist();
    });
  });
});
