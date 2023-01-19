/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);

const EagerEvaluation = createFactory(
  require("resource://devtools/client/webconsole/components/Input/EagerEvaluation.js")
);

const {
  setupStore,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");
const {
  SET_TERMINAL_EAGER_RESULT,
} = require("resource://devtools/client/webconsole/constants.js");

const {
  stubPackets,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");

function getEagerEvaluation(overrides = {}) {
  return EagerEvaluation({
    highlightDomElement: () => {},
    unHighlightDomElement: () => {},
    ...overrides,
  });
}

describe("EagerEvaluation component:", () => {
  it("render Date result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: stubPackets.get("new Date(0)").result,
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe(
      "Date Thu Jan 01 1970 01:00:00 GMT+0100 (Central European Standard Time)"
    );
  });

  it("render falsy integer (0) result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: 0,
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe("0");
  });

  it("render false result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: false,
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe("false");
  });

  it("render empty string result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: "",
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe(`""`);
  });

  it("render null grip result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: { type: "null" },
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe("null");
  });

  it("render undefined grip result", () => {
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: { type: "undefined" },
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(1);
    expect(wrapper.text()).toBe("undefined");
  });

  it("do not render null result", () => {
    // This is not to be confused with a grip describing `null` (which is {type: "null"})
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: null,
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(0);
    expect(wrapper.text()).toBe("");
  });

  it("do not render undefined result", () => {
    // This is not to be confused with a grip describing `undefined` (which is {type: "undefined"})
    const store = setupStore();
    store.dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: undefined,
    });

    const wrapper = render(Provider({ store }, getEagerEvaluation()));

    expect(wrapper.hasClass("eager-evaluation-result")).toBe(true);
    expect(wrapper.find(".eager-evaluation-result__row").length).toBe(0);
    expect(wrapper.text()).toBe("");
  });
});
