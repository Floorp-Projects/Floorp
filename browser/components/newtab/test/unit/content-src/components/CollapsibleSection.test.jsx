import { _CollapsibleSection as CollapsibleSection } from "content-src/components/CollapsibleSection/CollapsibleSection";
import { ErrorBoundary } from "content-src/components/ErrorBoundary/ErrorBoundary";
import { mount } from "enzyme";
import React from "react";

const DEFAULT_PROPS = {
  id: "cool",
  className: "cool-section",
  title: "Cool Section",
  prefName: "collapseSection",
  collapsed: false,
  eventSource: "foo",
  document: {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "visible",
  },
  dispatch: () => {},
  Prefs: { values: { featureConfig: {} } },
};

describe("CollapsibleSection", () => {
  let wrapper;

  function setup(props = {}) {
    const customProps = Object.assign({}, DEFAULT_PROPS, props);
    wrapper = mount(
      <CollapsibleSection {...customProps}>foo</CollapsibleSection>
    );
  }

  beforeEach(() => setup());

  it("should render the component", () => {
    assert.ok(wrapper.exists());
  });

  it("should render an ErrorBoundary with class section-body-fallback", () => {
    assert.equal(
      wrapper.find(ErrorBoundary).first().prop("className"),
      "section-body-fallback"
    );
  });

  describe("without collapsible pref", () => {
    let dispatch;
    beforeEach(() => {
      dispatch = sinon.stub();
      setup({ collapsed: undefined, dispatch });
    });
    it("should render the section uncollapsed", () => {
      assert.isFalse(
        wrapper.find(".collapsible-section").first().hasClass("collapsed")
      );
    });

    it("should not render the arrow if no collapsible pref exists for the section", () => {
      assert.lengthOf(wrapper.find(".click-target .collapsible-arrow"), 0);
    });
  });

  describe("icon", () => {
    it("no icon should be shown", () => {
      assert.lengthOf(wrapper.find(".icon"), 0);
    });
  });
});
