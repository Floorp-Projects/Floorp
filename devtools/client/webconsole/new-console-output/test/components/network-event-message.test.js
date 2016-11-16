/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
const NetworkEventMessage = createFactory(require("devtools/client/webconsole/new-console-output/components/message-types/network-event-message"));
const { INDENT_WIDTH } = require("devtools/client/webconsole/new-console-output/components/message-indent");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

const EXPECTED_URL = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/inexistent.html";

describe("NetworkEventMessage component:", () => {
  describe("GET request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("GET request");
      const wrapper = render(NetworkEventMessage({ message, serviceContainer }));
      const L10n = require("devtools/client/webconsole/new-console-output/test/fixtures/L10n");
      const { timestampString } = new L10n();

      expect(wrapper.find(".timestamp").text()).toBe(timestampString(message.timeStamp));
      expect(wrapper.find(".message-body .method").text()).toBe("GET");
      expect(wrapper.find(".message-body .xhr").length).toBe(0);
      expect(wrapper.find(".message-body .url").length).toBe(1);
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find("div.message.cm-s-mozilla span.message-body.devtools-monospace").length).toBe(1);
    });

    it("has the expected indent", () => {
      const message = stubPreparedMessages.get("GET request");

      const indent = 10;
      let wrapper = render(NetworkEventMessage({ message, serviceContainer, indent}));
      expect(wrapper.find(".indent").prop("style").width)
        .toBe(`${indent * INDENT_WIDTH}px`);

      wrapper = render(NetworkEventMessage({ message, serviceContainer }));
      expect(wrapper.find(".indent").prop("style").width).toBe(`0`);
    });
  });

  describe("XHR GET request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("XHR GET request");
      const wrapper = render(NetworkEventMessage({ message, serviceContainer }));

      expect(wrapper.find(".message-body .method").text()).toBe("GET");
      expect(wrapper.find(".message-body .xhr").length).toBe(1);
      expect(wrapper.find(".message-body .xhr").text()).toBe("XHR");
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find("div.message.cm-s-mozilla span.message-body.devtools-monospace").length).toBe(1);
    });
  });

  describe("XHR POST request", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("XHR POST request");
      const wrapper = render(NetworkEventMessage({ message, serviceContainer }));

      expect(wrapper.find(".message-body .method").text()).toBe("POST");
      expect(wrapper.find(".message-body .xhr").length).toBe(1);
      expect(wrapper.find(".message-body .xhr").text()).toBe("XHR");
      expect(wrapper.find(".message-body .url").length).toBe(1);
      expect(wrapper.find(".message-body .url").text()).toBe(EXPECTED_URL);
      expect(wrapper.find("div.message.cm-s-mozilla span.message-body.devtools-monospace").length).toBe(1);
    });
  });
});
