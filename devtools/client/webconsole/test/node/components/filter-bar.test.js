/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const sinon = require("sinon");
const { render, mount, shallow } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);

const actions = require("resource://devtools/client/webconsole/actions/index.js");
const FilterButton = require("resource://devtools/client/webconsole/components/FilterBar/FilterButton.js");
const FilterBar = createFactory(
  require("resource://devtools/client/webconsole/components/FilterBar/FilterBar.js")
);
const {
  FILTERBAR_DISPLAY_MODES,
} = require("resource://devtools/client/webconsole/constants.js");
const {
  MESSAGES_CLEAR,
  FILTERS,
} = require("resource://devtools/client/webconsole/constants.js");

const {
  setupStore,
  clearPrefs,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");
const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");

function getFilterBar(overrides = {}) {
  return FilterBar({
    serviceContainer,
    attachRefToWebConsoleUI: () => {},
    webConsoleUI: {
      document,
      wrapper: {},
    },
    ...overrides,
  });
}

describe("FilterBar component:", () => {
  afterEach(() => {
    clearPrefs();
  });

  it("initial render", () => {
    const store = setupStore();

    const wrapper = render(Provider({ store }, getFilterBar()));
    const toolbar = wrapper.find(
      ".devtools-toolbar.webconsole-filterbar-primary"
    );

    // Clear button
    const clearButton = toolbar.children().eq(0);
    expect(clearButton.attr("class")).toBe(
      "devtools-button devtools-clear-icon"
    );
    expect(clearButton.attr("title")).toBe("Clear the Web Console output");

    // Separator
    expect(toolbar.children().eq(1).attr("class")).toBe("devtools-separator");

    // Text filter
    const textInput = toolbar.children().eq(2);
    expect(textInput.attr("class")).toBe("devtools-searchbox");

    // Text filter input
    const textFilter = textInput.children().eq(0);
    expect(textFilter.attr("class")).toBe("devtools-filterinput");
    expect(textFilter.attr("placeholder")).toBe("Filter Output");
    expect(textFilter.attr("type")).toBe("search");
    expect(textFilter.attr("value")).toBe("");

    // Text filter input clear button
    const textFilterClearButton = textInput.children().eq(1);
    expect(textFilterClearButton.attr("class")).toBe(
      "devtools-searchinput-clear"
    );

    // Settings menu icon
    expect(
      wrapper.find(".webconsole-console-settings-menu-button").length
    ).toBe(1);
  });

  it("displays the number of hidden messages when a search hide messages", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
    ]);
    store.dispatch(actions.filterTextSet("qwerty"));

    const wrapper = mount(Provider({ store }, getFilterBar()));

    const message = wrapper.find(".devtools-searchinput-summary");
    expect(message.text()).toBe("5 hidden");
    expect(message.prop("title")).toBe("5 items hidden by text filter");
  });

  it("displays the number of hidden messages when a search hide 1 message", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.info('info message');",
    ]);
    store.dispatch(actions.filterTextSet("foobar"));

    const wrapper = mount(Provider({ store }, getFilterBar()));

    const message = wrapper.find(".devtools-searchinput-summary");
    expect(message.text()).toBe("1 hidden");
    expect(message.prop("title")).toBe("1 item hidden by text filter");
  });

  it("displays the expected number of hidden messages when multiple filters", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
    ]);
    store.dispatch(actions.filterTextSet("qwerty"));
    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.INFO));

    const wrapper = mount(Provider({ store }, getFilterBar()));

    const message = wrapper.find(".devtools-searchinput-summary");
    expect(message.text()).toBe("3 hidden");
    expect(message.prop("title")).toBe("3 items hidden by text filter");
  });

  it("does not display the number of hidden messages when there are no messages", () => {
    const store = setupStore();
    store.dispatch(actions.filterTextSet("qwerty"));
    const wrapper = mount(Provider({ store }, getFilterBar()));

    const toolbar = wrapper.find(".devtools-searchinput-summary");
    expect(toolbar.exists()).toBeFalsy();
  });

  it("Displays a filter buttons bar on its own element in narrow displayMode", () => {
    const store = setupStore();

    const wrapper = mount(
      Provider(
        { store },
        getFilterBar({
          displayMode: FILTERBAR_DISPLAY_MODES.NARROW,
        })
      )
    );

    const secondaryBar = wrapper.find(".webconsole-filterbar-secondary");
    expect(secondaryBar.length).toBe(1);

    // Buttons are displayed
    const filterBtn = props =>
      FilterButton(
        Object.assign(
          {},
          {
            active: true,
            dispatch: store.dispatch,
          },
          props
        )
      );

    const buttons = [
      filterBtn({ label: "Errors", filterKey: FILTERS.ERROR }),
      filterBtn({ label: "Warnings", filterKey: FILTERS.WARN }),
      filterBtn({ label: "Logs", filterKey: FILTERS.LOG }),
      filterBtn({ label: "Info", filterKey: FILTERS.INFO }),
      filterBtn({ label: "Debug", filterKey: FILTERS.DEBUG }),
      dom.div({
        className: "devtools-separator",
      }),
      filterBtn({
        label: "CSS",
        filterKey: "css",
        active: false,
        title:
          "Stylesheets will be reparsed to check for errors. Refresh the page to also see errors from stylesheets modified from Javascript.",
      }),
      filterBtn({ label: "XHR", filterKey: "netxhr", active: false }),
      filterBtn({ label: "Requests", filterKey: "net", active: false }),
    ];

    secondaryBar.children().forEach((child, index) => {
      expect(child.html()).toEqual(shallow(buttons[index]).html());
    });
  });

  it("fires MESSAGES_CLEAR action when clear button is clicked", () => {
    const store = setupStore();
    store.dispatch = sinon.spy();

    const wrapper = mount(Provider({ store }, getFilterBar()));
    wrapper.find(".devtools-clear-icon").simulate("click");
    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      type: MESSAGES_CLEAR,
    });
  });

  it("sets filter text when text is typed", () => {
    const store = setupStore();

    const wrapper = mount(Provider({ store }, getFilterBar()));
    const input = wrapper.find(".devtools-filterinput");
    input.simulate("change", { target: { value: "a" } });
    expect(store.getState().filters.text).toBe("a");
  });
});
