/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render, mount } = require("enzyme");
const sinon = require("sinon");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("react-redux").Provider);
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");

// Components under test.
const ConsoleApiCall = createFactory(require("devtools/client/webconsole/new-console-output/components/message-types/console-api-call"));
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("devtools/client/webconsole/new-console-output/constants");
const { INDENT_WIDTH } = require("devtools/client/webconsole/new-console-output/components/message-indent");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("ConsoleAPICall component:", () => {
  describe("console.log", () => {
    it("renders string grips", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("foobar test");
      expect(wrapper.find(".objectBox-string").length).toBe(2);
      let selector = "div.message.cm-s-mozilla span span.message-flex-body " +
        "span.message-body.devtools-monospace";
      expect(wrapper.find(selector).length).toBe(1);

      // There should be the location
      const locationLink = wrapper.find(`.message-location`);
      expect(locationLink.length).toBe(1);
      expect(locationLink.text()).toBe("test-console-api.html:1:27");
    });

    it("renders string grips with custom style", () => {
      const message = stubPreparedMessages.get("console.log(%cfoobar)");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      const elements = wrapper.find(".objectBox-string");
      expect(elements.text()).toBe("foobar");
      expect(elements.length).toBe(2);

      const firstElementStyle = elements.eq(0).prop("style");
      // Allowed styles are applied accordingly on the first element.
      expect(firstElementStyle.color).toBe(`blue`);
      expect(firstElementStyle["font-size"]).toBe(`1.3em`);
      // Forbidden styles are not applied.
      expect(firstElementStyle["background-image"]).toBe(undefined);
      expect(firstElementStyle.position).toBe(undefined);
      expect(firstElementStyle.top).toBe(undefined);

      const secondElementStyle = elements.eq(1).prop("style");
      // Allowed styles are applied accordingly on the second element.
      expect(secondElementStyle.color).toBe(`red`);
      // Forbidden styles are not applied.
      expect(secondElementStyle.background).toBe(undefined);
    });

    it("renders repeat node", () => {
      const message =
        stubPreparedMessages.get("console.log('foobar', 'test')")
        .set("repeat", 107);
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-repeats").text()).toBe("107");
      expect(wrapper.find(".message-repeats").prop("title")).toBe("107 repeats");

      let selector = "span > span.message-flex-body > " +
        "span.message-body.devtools-monospace + span.message-repeats";
      expect(wrapper.find(selector).length).toBe(1);
    });

    it("has the expected indent", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");

      const indent = 10;
      let wrapper = render(ConsoleApiCall({ message, serviceContainer, indent }));
      expect(wrapper.find(".indent").prop("style").width)
        .toBe(`${indent * INDENT_WIDTH}px`);

      wrapper = render(ConsoleApiCall({ message, serviceContainer}));
      expect(wrapper.find(".indent").prop("style").width).toBe(`0`);
    });

    it("renders a timestamp when passed a truthy timestampsVisible prop", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({
        message,
        serviceContainer,
        timestampsVisible: true,
      }));
      const L10n = require("devtools/client/webconsole/new-console-output/test/fixtures/L10n");
      const { timestampString } = new L10n();

      expect(wrapper.find(".timestamp").text()).toBe(timestampString(message.timeStamp));
    });

    it("does not render a timestamp when not asked to", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({
        message,
        serviceContainer,
      }));

      expect(wrapper.find(".timestamp").length).toBe(0);
    });
  });

  describe("console.count", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.count('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("bar: 1");
    });
  });

  describe("console.assert", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get(
        "console.assert(false, {message: 'foobar'})");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text())
        .toBe("Assertion failed: Object { message: \"foobar\" }");
    });
  });

  describe("console.time", () => {
    it("does not show anything", () => {
      const message = stubPreparedMessages.get("console.time('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("");
    });
  });

  describe("console.timeEnd", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("console.timeEnd('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe(message.messageText);
      expect(wrapper.find(".message-body").text()).toMatch(/^bar: \d+(\.\d+)?ms$/);
    });
  });

  describe("console.trace", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.trace()");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer, open: true }));
      const filepath = "http://example.com/browser/devtools/client/webconsole/" +
                       "new-console-output/test/fixtures/stub-generators/" +
                       "test-console-api.html";

      expect(wrapper.find(".message-body").text()).toBe("console.trace()");

      const frameLinks = wrapper.find(
        `.stack-trace span.frame-link[data-url]`);
      expect(frameLinks.length).toBe(3);

      expect(frameLinks.eq(0).find(".frame-link-function-display-name").text())
        .toBe("testStacktraceFiltering");
      expect(frameLinks.eq(0).find(".frame-link-filename").text())
        .toBe(filepath);

      expect(frameLinks.eq(1).find(".frame-link-function-display-name").text())
        .toBe("foo");
      expect(frameLinks.eq(1).find(".frame-link-filename").text())
        .toBe(filepath);

      expect(frameLinks.eq(2).find(".frame-link-function-display-name").text())
        .toBe("triggerPacket");
      expect(frameLinks.eq(2).find(".frame-link-filename").text())
        .toBe(filepath);

      // it should not be collapsible.
      expect(wrapper.find(`.theme-twisty`).length).toBe(0);
    });
  });

  describe("console.group", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.group('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer, open: true }));

      expect(wrapper.find(".message-body").text()).toBe("bar");
      expect(wrapper.find(".theme-twisty.open").length).toBe(1);
    });

    it("renders group with custom style", () => {
      const message = stubPreparedMessages.get("console.group(%cfoo%cbar)");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      const elements = wrapper.find(".objectBox-string");
      expect(elements.text()).toBe("foobar");
      expect(elements.length).toBe(2);

      const firstElementStyle = elements.eq(0).prop("style");
      // Allowed styles are applied accordingly on the first element.
      expect(firstElementStyle.color).toBe(`blue`);
      expect(firstElementStyle["font-size"]).toBe(`1.3em`);
      // Forbidden styles are not applied.
      expect(firstElementStyle["background-image"]).toBe(undefined);
      expect(firstElementStyle.position).toBe(undefined);
      expect(firstElementStyle.top).toBe(undefined);

      const secondElementStyle = elements.eq(1).prop("style");
      // Allowed styles are applied accordingly on the second element.
      expect(secondElementStyle.color).toBe(`red`);
      // Forbidden styles are not applied.
      expect(secondElementStyle.background).toBe(undefined);
    });

    it("toggle the group when the collapse button is clicked", () => {
      const store = setupStore([]);
      store.dispatch = sinon.spy();
      const message = stubPreparedMessages.get("console.group('bar')");

      let wrapper = mount(Provider({store},
        ConsoleApiCall({
          message,
          open: true,
          dispatch: store.dispatch,
          serviceContainer,
        })
      ));
      wrapper.find(".theme-twisty.open").simulate("click");
      let call = store.dispatch.getCall(0);
      expect(call.args[0]).toEqual({
        id: message.id,
        type: MESSAGE_CLOSE
      });

      wrapper = mount(Provider({store},
        ConsoleApiCall({
          message,
          open: false,
          dispatch: store.dispatch,
          serviceContainer,
        })
      ));
      wrapper.find(".theme-twisty").simulate("click");
      call = store.dispatch.getCall(1);
      expect(call.args[0]).toEqual({
        id: message.id,
        type: MESSAGE_OPEN
      });
    });
  });

  describe("console.groupEnd", () => {
    it("does not show anything", () => {
      const message = stubPreparedMessages.get("console.groupEnd('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("");
    });
  });

  describe("console.groupCollapsed", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.groupCollapsed('foo')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer, open: false}));

      expect(wrapper.find(".message-body").text()).toBe("foo");
      expect(wrapper.find(".theme-twisty:not(.open)").length).toBe(1);
    });

    it("renders group with custom style", () => {
      const message = stubPreparedMessages.get("console.groupCollapsed(%cfoo%cbaz)");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      const elements = wrapper.find(".objectBox-string");
      expect(elements.text()).toBe("foobaz");
      expect(elements.length).toBe(2);

      const firstElementStyle = elements.eq(0).prop("style");
      // Allowed styles are applied accordingly on the first element.
      expect(firstElementStyle.color).toBe(`blue`);
      expect(firstElementStyle["font-size"]).toBe(`1.3em`);
      // Forbidden styles are not applied.
      expect(firstElementStyle["background-image"]).toBe(undefined);
      expect(firstElementStyle.position).toBe(undefined);
      expect(firstElementStyle.top).toBe(undefined);

      const secondElementStyle = elements.eq(1).prop("style");
      // Allowed styles are applied accordingly on the second element.
      expect(secondElementStyle.color).toBe(`red`);
      // Forbidden styles are not applied.
      expect(secondElementStyle.background).toBe(undefined);
    });
  });

  describe("console.dirxml", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.dirxml(window)");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text())
        .toBe("Window http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-console-api.html");
    });
  });
});
