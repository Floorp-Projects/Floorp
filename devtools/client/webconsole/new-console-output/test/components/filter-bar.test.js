/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const sinon = require("sinon");
const { render, mount, shallow } = require("enzyme");

const { createFactory, DOM } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("react-redux").Provider);

const FilterButton = require("devtools/client/webconsole/new-console-output/components/filter-button");
const FilterBar = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-bar"));
const { getAllUi } = require("devtools/client/webconsole/new-console-output/selectors/ui");
const {
  MESSAGES_CLEAR,
  MESSAGE_LEVEL
} = require("devtools/client/webconsole/new-console-output/constants");

const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("FilterBar component:", () => {
  it("initial render", () => {
    const store = setupStore([]);

    const wrapper = render(Provider({store}, FilterBar({ serviceContainer })));
    const toolbar = wrapper.find(
      ".devtools-toolbar.webconsole-filterbar-primary"
    );

    // Clear button
    expect(toolbar.children().eq(0).attr("class"))
      .toBe("devtools-button devtools-clear-icon");
    expect(toolbar.children().eq(0).attr("title")).toBe("Clear the Web Console output");

    // Filter bar toggle
    expect(toolbar.children().eq(1).attr("class"))
      .toBe("devtools-button devtools-filter-icon");
    expect(toolbar.children().eq(1).attr("title")).toBe("Toggle filter bar");

    // Text filter
    expect(toolbar.children().eq(2).attr("class"))
      .toBe("devtools-plaininput text-filter");
    expect(toolbar.children().eq(2).attr("placeholder")).toBe("Filter output");
    expect(toolbar.children().eq(2).attr("type")).toBe("search");
    expect(toolbar.children().eq(2).attr("value")).toBe("");
  });

  it("displays filter bar when button is clicked", () => {
    const store = setupStore([]);

    expect(getAllUi(store.getState()).filterBarVisible).toBe(false);

    const wrapper = mount(Provider({store}, FilterBar({ serviceContainer })));
    wrapper.find(".devtools-filter-icon").simulate("click");

    expect(getAllUi(store.getState()).filterBarVisible).toBe(true);

    const secondaryBar = wrapper.find(".webconsole-filterbar-secondary");
    expect(secondaryBar.length).toBe(1);

    // Buttons are displayed
    const filterBtn = props => FilterButton(
      Object.assign({}, {
        active: true,
        dispatch: store.dispatch
      }, props)
    );

    let buttons = [
      filterBtn({ label: "Errors", filterKey: MESSAGE_LEVEL.ERROR }),
      filterBtn({ label: "Warnings", filterKey: MESSAGE_LEVEL.WARN }),
      filterBtn({ label: "Logs", filterKey: MESSAGE_LEVEL.LOG }),
      filterBtn({ label: "Info", filterKey: MESSAGE_LEVEL.INFO }),
      filterBtn({ label: "Debug", filterKey: MESSAGE_LEVEL.DEBUG }),
      DOM.span({
        className: "devtools-separator",
      }),
      filterBtn({ label: "CSS", filterKey: "css" }),
      filterBtn({ label: "XHR", filterKey: "netxhr", active: false }),
      filterBtn({ label: "Requests", filterKey: "net", active: false }),
    ];

    secondaryBar.children().forEach((child, index) => {
      expect(child.html()).toEqual(shallow(buttons[index]).html());
    });
  });

  it("fires MESSAGES_CLEAR action when clear button is clicked", () => {
    const store = setupStore([]);
    store.dispatch = sinon.spy();

    const wrapper = mount(Provider({store}, FilterBar({ serviceContainer })));
    wrapper.find(".devtools-clear-icon").simulate("click");
    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      type: MESSAGES_CLEAR
    });
  });

  it("sets filter text when text is typed", () => {
    const store = setupStore([]);

    const wrapper = mount(Provider({store}, FilterBar({ serviceContainer })));
    wrapper.find(".devtools-plaininput").simulate("input", { target: { value: "a" } });
    expect(store.getState().filters.text).toBe("a");
  });
});
