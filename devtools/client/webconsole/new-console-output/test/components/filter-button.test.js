/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Require helper is necessary to load certain modules.
require("devtools/client/webconsole/new-console-output/test/requireHelper")();
const { render, shallow } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const FilterButton = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-button").FilterButton);
const {
  FILTER_TOGGLE,
  MESSAGE_LEVEL
} = require("devtools/client/webconsole/new-console-output/constants");

const expect = require("expect");
const sinon = require("sinon");

describe("FilterButton component:", () => {
  const props = {
    active: true,
    label: "Error",
    filterKey: MESSAGE_LEVEL.ERROR,
    dispatch: sinon.spy()
  };

  it("displays as active when turned on", () => {
    const wrapper = render(FilterButton(props));
    expect(wrapper.html()).toBe(
      "<button class=\"menu-filter-button checked\">Error</button>"
    );
  });

  it("displays as inactive when turned off", () => {
    const inactiveProps = Object.assign({}, props, { active: false });
    const wrapper = render(FilterButton(inactiveProps));
    expect(wrapper.html()).toBe(
      "<button class=\"menu-filter-button\">Error</button>"
    );
  });

  it("fires FILTER_TOGGLE action when clicked", () => {
    const wrapper = shallow(FilterButton(props));
    wrapper.find("button").simulate("click");
    const call = props.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      type: FILTER_TOGGLE,
      filter: MESSAGE_LEVEL.ERROR
    });
  });
});
