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
const { setupStore } = require("devtools/client/webconsole/test/node/helpers");

// Components under test.
const EvaluationResult = createFactory(
  require("devtools/client/webconsole/components/Output/message-types/EvaluationResult")
);
const {
  INDENT_WIDTH,
} = require("devtools/client/webconsole/components/Output/MessageIndent");

// Test fakes.
const {
  stubPreparedMessages,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/node/fixtures/serviceContainer");

describe("EvaluationResult component:", () => {
  it.skip("renders a grip result", () => {
    const message = stubPreparedMessages.get("new Date(0)");
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );

    expect(wrapper.find(".message-body").text()).toBe(
      "Date 1970-01-01T00:00:00.000Z"
    );

    expect(wrapper.hasClass("message")).toBe(true);
    expect(wrapper.hasClass("log")).toBe(true);
  });

  it("renders an error", () => {
    const message = stubPreparedMessages.get("asdf()");
    const wrapper = render(EvaluationResult({ message, serviceContainer }));

    expect(wrapper.find(".message-body").text()).toBe(
      "Uncaught ReferenceError: asdf is not defined[Learn More]"
    );

    expect(wrapper.hasClass("message")).toBe(true);
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("renders an error with a longString exception message", () => {
    const message = stubPreparedMessages.get("longString message Error");
    const wrapper = render(EvaluationResult({ message, serviceContainer }));

    const text = wrapper.find(".message-body").text();
    expect(text.startsWith("Uncaught Error: Long error Long error")).toBe(true);
    expect(wrapper.hasClass("message")).toBe(true);
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("renders thrown empty string", () => {
    const message = stubPreparedMessages.get(`eval throw ""`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught <empty string>");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("renders thrown string", () => {
    const message = stubPreparedMessages.get(`eval throw "tomato"`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught tomato");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown Boolean", () => {
    const message = stubPreparedMessages.get(`eval throw false`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught false");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown Number", () => {
    const message = stubPreparedMessages.get(`eval throw 0`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught 0");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown null", () => {
    const message = stubPreparedMessages.get(`eval throw null`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught null");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown undefined", () => {
    const message = stubPreparedMessages.get(`eval throw undefined`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught undefined");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown Symbol", () => {
    const message = stubPreparedMessages.get(`eval throw Symbol`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe('Uncaught Symbol("potato")');
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown Object", () => {
    const message = stubPreparedMessages.get(`eval throw Object`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Object { vegetable: "cucumber" }`);
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown Error Object", () => {
    const message = stubPreparedMessages.get(`eval throw Error Object`);
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught Error: pumpkin");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render thrown ErrorObject with custom name", () => {
    const message = stubPreparedMessages.get(
      `eval throw Error Object with custom name`
    );
    const wrapper = render(EvaluationResult({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught JuicyError: pineapple");
    expect(wrapper.hasClass("error")).toBe(true);
  });

  it("render pending Promise", () => {
    const message = stubPreparedMessages.get(`eval pending promise`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Promise { <state>: "pending" }`);
  });

  it("render Promise.resolve result", () => {
    const message = stubPreparedMessages.get(`eval Promise.resolve`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Promise { <state>: "fulfilled", <value>: 123 }`);
  });

  it("render Promise.reject result", () => {
    const message = stubPreparedMessages.get(`eval Promise.reject`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Promise { <state>: "rejected", <reason>: "ouch" }`);
  });

  it("render promise fulfilled in microtask", () => {
    // See Bug 1439963
    const message = stubPreparedMessages.get(`eval resolved promise`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Promise { <state>: "fulfilled", <value>: 246 }`);
  });

  it("render promise rejected in microtask", () => {
    // See Bug 1439963
    const message = stubPreparedMessages.get(`eval rejected promise`);
    // We need to wrap the EvaluationResult in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(
      `Promise { <state>: "rejected", <reason>: ReferenceError }`
    );
  });

  it("renders an inspect command result", () => {
    const message = stubPreparedMessages.get("inspect({a: 1})");
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );

    expect(wrapper.find(".message-body").text()).toBe("Object { a: 1 }");
  });

  it("displays a [Learn more] link", () => {
    const store = setupStore();

    const message = stubPreparedMessages.get("asdf()");

    serviceContainer.openLink = sinon.spy();
    const wrapper = mount(
      Provider(
        { store },
        EvaluationResult({
          message,
          serviceContainer,
          dispatch: () => {},
        })
      )
    );

    const url =
      "https://developer.mozilla.org/docs/Web/JavaScript/Reference/Errors/Not_defined";
    const learnMore = wrapper.find(".learn-more-link");
    expect(learnMore.length).toBe(1);
    expect(learnMore.prop("title")).toBe(url);

    learnMore.simulate("click");
    const call = serviceContainer.openLink.getCall(0);
    expect(call.args[0]).toEqual(message.exceptionDocURL);
  });

  it("has the expected indent", () => {
    const message = stubPreparedMessages.get("new Date(0)");

    const indent = 10;
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    let wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({
          message: Object.assign({}, message, { indent }),
          serviceContainer,
        })
      )
    );
    let indentEl = wrapper.find(".indent");
    expect(indentEl.prop("style").width).toBe(`${indent * INDENT_WIDTH}px`);
    expect(indentEl.prop("data-indent")).toBe(`${indent}`);

    wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({ message, serviceContainer })
      )
    );
    indentEl = wrapper.find(".indent");
    expect(indentEl.prop("style").width).toBe(`0`);
    expect(indentEl.prop("data-indent")).toBe(`0`);
  });

  it("has location information", () => {
    const message = stubPreparedMessages.get("1 + @");
    const wrapper = render(EvaluationResult({ message, serviceContainer }));

    const locationLink = wrapper.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("debugger eval code:1:4");
  });

  it("has a timestamp when passed a truthy timestampsVisible prop", () => {
    const message = stubPreparedMessages.get("new Date(0)");
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({
          message,
          serviceContainer,
          timestampsVisible: true,
        })
      )
    );
    const {
      timestampString,
    } = require("devtools/client/webconsole/utils/l10n");

    expect(wrapper.find(".timestamp").text()).toBe(
      timestampString(message.timeStamp)
    );
  });

  it("does not have a timestamp when timestampsVisible prop is falsy", () => {
    const message = stubPreparedMessages.get("new Date(0)");
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        EvaluationResult({
          message,
          serviceContainer,
          timestampsVisible: false,
        })
      )
    );

    expect(wrapper.find(".timestamp").length).toBe(0);
  });
});
