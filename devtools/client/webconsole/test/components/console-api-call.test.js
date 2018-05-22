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
const { setupStore } = require("devtools/client/webconsole/test/helpers");

// Components under test.
const ConsoleApiCall = createFactory(require("devtools/client/webconsole/components/message-types/ConsoleApiCall"));
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("devtools/client/webconsole/constants");
const { INDENT_WIDTH } = require("devtools/client/webconsole/components/MessageIndent");
const {prepareMessage} = require("devtools/client/webconsole/utils/messages");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");

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
      expect(secondElementStyle["line-height"]).toBe("1.5");
      // Forbidden styles are not applied.
      expect(secondElementStyle.background).toBe(undefined);
    });

    it("renders custom styled logs with empty style as expected", () => {
      const message = stubPreparedMessages.get('console.log("%cHello%c|%cWorld")');
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      const elements = wrapper.find(".objectBox-string");
      expect(elements.text()).toBe("Hello|World");
      expect(elements.length).toBe(3);

      const firstElementStyle = elements.eq(0).prop("style");
      // Allowed styles are applied accordingly on the first element.
      expect(firstElementStyle.color).toBe("red");

      const secondElementStyle = elements.eq(1).prop("style");
      expect(secondElementStyle.color).toBe(undefined);

      const thirdElementStyle = elements.eq(2).prop("style");
      // Allowed styles are applied accordingly on the third element.
      expect(thirdElementStyle.color).toBe("blue");
    });

    it("renders prefixed messages", () => {
      const stub = {
        "level": "debug",
        "filename": "resource:///modules/CustomizableUI.jsm",
        "lineNumber": 181,
        "functionName": "initialize",
        "timeStamp": 1519311532912,
        "arguments": [
          "Initializing"
        ],
        "prefix": "MyNicePrefix",
        "workerType": "none",
        "styles": [],
        "category": "webdev",
        "_type": "ConsoleAPI"
      };
      const wrapper = render(ConsoleApiCall({
        message: prepareMessage(stub, {getNextId: () => "p"}),
        serviceContainer
      }));
      const prefix = wrapper.find(".console-message-prefix");
      expect(prefix.text()).toBe("MyNicePrefix: ");

      expect(wrapper.find(".message-body").text()).toBe("MyNicePrefix: Initializing");

      // There should be the location
      const locationLink = wrapper.find(`.message-location`);
      expect(locationLink.length).toBe(1);
      expect(locationLink.text()).toBe("CustomizableUI.jsm:181");
    });

    it("renders repeat node", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({
        message,
        serviceContainer,
        repeat: 107
      }));

      expect(wrapper.find(".message-repeats").text()).toBe("107");
      expect(wrapper.find(".message-repeats").prop("title")).toBe("107 repeats");

      let selector = "span > span.message-flex-body > " +
        "span.message-body.devtools-monospace + span.message-repeats";
      expect(wrapper.find(selector).length).toBe(1);
    });

    it("has the expected indent", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");

      const indent = 10;
      let wrapper = render(ConsoleApiCall({
        message: Object.assign({}, message, {indent}),
        serviceContainer
      }));
      let indentEl = wrapper.find(".indent");
      expect(indentEl.prop("style").width).toBe(`${indent * INDENT_WIDTH}px`);
      expect(indentEl.prop("data-indent")).toBe(`${indent}`);

      wrapper = render(ConsoleApiCall({ message, serviceContainer}));
      indentEl = wrapper.find(".indent");
      expect(indentEl.prop("style").width).toBe(`0`);
      expect(indentEl.prop("data-indent")).toBe(`0`);
    });

    it("renders a timestamp when passed a truthy timestampsVisible prop", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({
        message,
        serviceContainer,
        timestampsVisible: true,
      }));
      const { timestampString } = require("devtools/client/webconsole/webconsole-l10n");

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
      const messages = [{
        key: "console.count('bar')",
        expectedBodyText: "bar: 1",
      }, {
        key: "console.count | default: 1",
        expectedBodyText: "default: 1",
      }, {
        key: "console.count | default: 2",
        expectedBodyText: "default: 2",
      }, {
        key: "console.count | test counter: 1",
        expectedBodyText: "test counter: 1",
      }, {
        key: "console.count | test counter: 2",
        expectedBodyText: "test counter: 2",
      }, {
        key: "console.count | default: 3",
        expectedBodyText: "default: 3",
      }, {
        key: "console.count | default: 4",
        expectedBodyText: "default: 4",
      }, {
        key: "console.count | test counter: 3",
        expectedBodyText: "test counter: 3",
      }, {
        key: "console.countReset | test counter: 0",
        expectedBodyText: "test counter: 0",
      }, {
        key: "console.countReset | counterDoesntExist",
        expectedBodyText: "Counter “test counter” doesn’t exist.",
      }];

      for (const {key, expectedBodyText} of messages) {
        const message = stubPreparedMessages.get(key);
        const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

        expect(wrapper.find(".message-body").text()).toBe(expectedBodyText);
      }
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
    it("shows an error if called again", () => {
      const message = stubPreparedMessages.get("timerAlreadyExists");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("Timer “bar” already exists.");
    });
  });

  describe("console.timeLog", () => {
    it("renders as expected", () => {
      let message = stubPreparedMessages.get("console.timeLog('bar') - 1");
      let wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe(message.parameters[0]);
      expect(wrapper.find(".message-body").text()).toMatch(/^bar: \d+(\.\d+)?ms$/);

      message = stubPreparedMessages.get("console.timeLog('bar') - 2");
      wrapper = render(ConsoleApiCall({ message, serviceContainer }));
      expect(wrapper.find(".message-body").text())
        .toMatch(/^bar: \d+(\.\d+)?ms second call Object \{ state\: 1 \}$/);
    });
    it("shows an error if the timer doesn't exist", () => {
      const message = stubPreparedMessages.get("timeLog.timerDoesntExist");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("Timer “bar” doesn’t exist.");
    });
  });

  describe("console.timeEnd", () => {
    it("renders as expected", () => {
      const message = stubPreparedMessages.get("console.timeEnd('bar')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe(message.messageText);
      expect(wrapper.find(".message-body").text()).toMatch(/^bar: \d+(\.\d+)?ms$/);
    });
    it("shows an error if the timer doesn't exist", () => {
      const message = stubPreparedMessages.get("timeEnd.timerDoesntExist");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("Timer “bar” doesn’t exist.");
    });
  });

  describe("console.trace", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.trace()");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer, open: true }));
      const filepath = "http://example.com/browser/devtools/client/webconsole/" +
                       "test/fixtures/stub-generators/" +
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
      expect(wrapper.find(".collapse-button.expanded").length).toBe(1);
    });

    it("renders group with custom style", () => {
      const message = stubPreparedMessages.get("console.group(%cfoo%cbar)");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));
      expect(wrapper.find(".message-body").text()).toBe("foobar");

      const elements = wrapper.find(".objectBox-string");
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
      const store = setupStore();
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
      wrapper.find(".collapse-button.expanded").simulate("click");
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
      wrapper.find(".collapse-button").simulate("click");
      call = store.dispatch.getCall(1);
      expect(call.args[0]).toEqual({
        id: message.id,
        type: MESSAGE_OPEN
      });
    });

    it("toggle the group when the group name is clicked", () => {
      const store = setupStore();
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
      wrapper.find(".message-flex-body").simulate("click");
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
      wrapper.find(".message-flex-body").simulate("click");
      call = store.dispatch.getCall(1);
      expect(call.args[0]).toEqual({
        id: message.id,
        type: MESSAGE_OPEN
      });
    });

    it("doesn't toggle the group when the location link is clicked", () => {
      const store = setupStore();
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
      wrapper.find(".frame-link-source").simulate("click");
      let call = store.dispatch.getCall(0);
      expect(call).toNotExist();
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
      expect(wrapper.find(".collapse-button:not(.expanded)").length).toBe(1);
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
        .toBe("Window http://example.com/browser/devtools/client/webconsole/test/fixtures/stub-generators/test-console-api.html");
    });
  });

  describe("console.dir", () => {
    it("renders", () => {
      const message = stubPreparedMessages.get("console.dir({C, M, Y, K})");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text())
        .toBe(`Object { cyan: "C", magenta: "M", yellow: "Y", black: "K" }`);
    });
  });
});
