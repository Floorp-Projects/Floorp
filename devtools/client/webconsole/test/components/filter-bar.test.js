/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const sinon = require("sinon");
const { render, mount, shallow } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Provider = createFactory(require("react-redux").Provider);

const actions = require("devtools/client/webconsole/actions/index");
const FilterButton = require("devtools/client/webconsole/components/FilterButton");
const FilterBar = createFactory(require("devtools/client/webconsole/components/FilterBar"));
const { getAllUi } = require("devtools/client/webconsole/selectors/ui");
const { getAllFilters } = require("devtools/client/webconsole/selectors/filters");
const {
  MESSAGES_CLEAR,
  FILTERS,
  PREFS,
} = require("devtools/client/webconsole/constants");

const { setupStore, prefsService, clearPrefs } = require("devtools/client/webconsole/test/helpers");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");

describe("FilterBar component:", () => {
  afterEach(() => {
    clearPrefs();
  });

  it("initial render", () => {
    const store = setupStore();

    const wrapper = render(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const toolbar = wrapper.find(".devtools-toolbar.webconsole-filterbar-primary");

    // Clear button
    const clearButton = toolbar.children().eq(0);
    expect(clearButton.attr("class")).toBe("devtools-button devtools-clear-icon");
    expect(clearButton.attr("title")).toBe("Clear the Web Console output");

    // Separator
    expect(toolbar.children().eq(1).attr("class")).toBe("devtools-separator");

    // Filter bar toggle
    const filterBarButton = toolbar.children().eq(2);
    expect(filterBarButton.attr("class")).toBe("devtools-button devtools-filter-icon");
    expect(filterBarButton.attr("title")).toBe("Toggle filter bar");

    // Text filter
    const textFilter = toolbar.children().eq(3);
    expect(textFilter.attr("class")).toBe("devtools-plaininput text-filter");
    expect(textFilter.attr("placeholder")).toBe("Filter output");
    expect(textFilter.attr("type")).toBe("search");
    expect(textFilter.attr("value")).toBe("");

    // "Persist logs" checkbox
    expect(wrapper.find(".filter-checkbox input").length).toBe(1);
  });

  it("displays the number of hidden messages when there are one hidden message", () => {
    const store = setupStore([
      "console.log('foobar', 'test')"
    ]);
    // Filter-out LOG messages
    store.dispatch(actions.filterToggle(FILTERS.LOG));

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeTruthy();

    const message = toolbar.find(".filter-message-text");
    expect(message.text()).toBe("1 item hidden by filters");
    expect(message.prop("title")).toBe("log: 1");
  });

  it("Reset filters when the Reset filters button is clicked.", () => {
    const store = setupStore([
      "console.log('foobar', 'test')"
    ]);
    // Filter-out LOG messages
    store.dispatch(actions.filterToggle(FILTERS.LOG));
    const wrapper = mount(Provider({store}, FilterBar({serviceContainer})));

    const resetFiltersButton = wrapper.find(
      ".webconsole-filterbar-filtered-messages .devtools-button");
    resetFiltersButton.simulate("click");

    // Toolbar is now hidden
    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeFalsy();
    expect(getAllFilters(store.getState())[FILTERS.LOG]).toBeTruthy();
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

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeTruthy();

    const message = toolbar.find(".filter-message-text");
    expect(message.text()).toBe("5 items hidden by filters");
    expect(message.prop("title")).toBe("text: 5");
  });

  it("displays the number of hidden messages when there are multiple ones", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
    ]);

    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.WARN));
    store.dispatch(actions.filterToggle(FILTERS.LOG));
    store.dispatch(actions.filterToggle(FILTERS.INFO));
    store.dispatch(actions.filterToggle(FILTERS.DEBUG));
    store.dispatch(actions.filterTextSet("qwerty"));

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const message = wrapper.find(".filter-message-text");

    expect(message.prop("title")).toBe("text: 10");
  });

  it("displays expected tooltip when there is text & level hidden-messages", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
      "console.log('foobar', 'test')",
      "console.info('info message');",
      "console.warn('danger, will robinson!')",
      "console.debug('debug message');",
      "console.error('error message');",
    ]);

    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.WARN));
    store.dispatch(actions.filterToggle(FILTERS.LOG));
    store.dispatch(actions.filterToggle(FILTERS.INFO));
    store.dispatch(actions.filterToggle(FILTERS.DEBUG));

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeTruthy();

    const message = toolbar.find(".filter-message-text");
    expect(message.text()).toBe("10 items hidden by filters");
    expect(message.prop("title")).toBe("error: 2, warn: 2, log: 2, info: 2, debug: 2");
  });

  it("does not display the number of hidden messages when there are no messages", () => {
    const store = setupStore();
    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeFalsy();
  });

  it("does not display the number of hidden non-default filters (CSS, Network,…)", () => {
    const store = setupStore([
      "Unknown property ‘such-unknown-property’.  Declaration dropped.",
      "GET request",
      "XHR GET request"
    ]);
    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));

    // Let's make sure those non-default filters are off.
    const filters = getAllFilters(store.getState());
    expect(filters[FILTERS.CSS]).toBe(false);
    expect(filters[FILTERS.NET]).toBe(false);
    expect(filters[FILTERS.NETXHR]).toBe(false);

    const toolbar = wrapper.find(".webconsole-filterbar-filtered-messages");
    expect(toolbar.exists()).toBeFalsy();
  });

  it("displays filter bar when button is clicked", () => {
    const store = setupStore();

    expect(getAllUi(store.getState()).filterBarVisible).toBe(false);
    expect(prefsService.getBoolPref(PREFS.UI.FILTER_BAR), false);

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    wrapper.find(".devtools-filter-icon").simulate("click");

    expect(getAllUi(store.getState()).filterBarVisible).toBe(true);
    expect(prefsService.getBoolPref(PREFS.UI.FILTER_BAR), true);

    const secondaryBar = wrapper.find(".webconsole-filterbar-secondary");
    expect(secondaryBar.length).toBe(1);

    // Buttons are displayed
    const filterBtn = props => FilterButton(
      Object.assign({}, {
        active: true,
        dispatch: store.dispatch
      }, props)
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
      filterBtn({ label: "CSS", filterKey: "css", active: false }),
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

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    wrapper.find(".devtools-clear-icon").simulate("click");
    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      type: MESSAGES_CLEAR
    });
  });

  it("sets filter text when text is typed", () => {
    const store = setupStore();

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    wrapper.find(".devtools-plaininput").simulate("input", { target: { value: "a" } });
    expect(store.getState().filters.text).toBe("a");
  });

  it("toggles persist logs when checkbox is clicked", () => {
    const store = setupStore();

    expect(getAllUi(store.getState()).persistLogs).toBe(false);
    expect(prefsService.getBoolPref(PREFS.UI.PERSIST), false);

    const wrapper = mount(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: false,
    })));
    wrapper.find(".filter-checkbox input").simulate("change");

    expect(getAllUi(store.getState()).persistLogs).toBe(true);
    expect(prefsService.getBoolPref(PREFS.UI.PERSIST), true);
  });

  it(`doesn't render "Persist logs" input when "hidePersistLogsCheckbox" is true`, () => {
    const store = setupStore();

    const wrapper = render(Provider({store}, FilterBar({
      serviceContainer,
      hidePersistLogsCheckbox: true,
    })));
    expect(wrapper.find(".filter-checkbox input").length).toBe(0);
  });
});
