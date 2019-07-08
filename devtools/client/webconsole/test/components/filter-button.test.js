/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const FilterButton = createFactory(
  require("devtools/client/webconsole/components/FilterBar/FilterButton")
);
const { MESSAGE_LEVEL } = require("devtools/client/webconsole/constants");

describe("FilterButton component:", () => {
  const props = {
    active: true,
    label: "Error",
    filterKey: MESSAGE_LEVEL.ERROR,
  };

  it("displays as active when turned on", () => {
    const wrapper = render(FilterButton(props));
    expect(wrapper.is("button")).toBe(true);
    expect(wrapper.hasClass("devtools-togglebutton")).toBe(true);
    expect(wrapper.hasClass("error")).toBe(true);
    expect(wrapper.attr("aria-pressed")).toBe("true");
    expect(wrapper.text()).toBe("Error");
  });

  it("displays as inactive when turned off", () => {
    const wrapper = render(FilterButton({ ...props, active: false }));
    expect(wrapper.is("button")).toBe(true);
    expect(wrapper.hasClass("devtools-togglebutton")).toBe(true);
    expect(wrapper.hasClass("error")).toBe(true);
    expect(wrapper.attr("aria-pressed")).toBe("false");
    expect(wrapper.text()).toBe("Error");
  });
});
