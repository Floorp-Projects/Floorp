import { actionCreators as ac } from "common/Actions.sys.mjs";
import { ContentSection } from "content-src/components/CustomizeMenu/ContentSection/ContentSection";
import { mount } from "enzyme";
import React from "react";

const DEFAULT_PROPS = {
  enabledSections: {
    pocketEnabled: true,
    topSitesEnabled: true,
  },
  mayHaveSponsoredTopSites: true,
  mayHaveSponsoredStories: true,
  pocketRegion: true,
  dispatch: sinon.stub(),
  setPref: sinon.stub(),
};

describe("ContentSection", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = mount(<ContentSection {...DEFAULT_PROPS} />);
  });

  it("should render the component", () => {
    assert.ok(wrapper.exists());
  });

  it("should look for a data-eventSource attribute and dispatch an event for INPUT", () => {
    wrapper.instance().onPreferenceSelect({
      target: {
        nodeName: "INPUT",
        checked: true,
        dataset: { preference: "foo", eventSource: "bar" },
      },
    });

    assert.calledWith(
      DEFAULT_PROPS.dispatch,
      ac.UserEvent({
        event: "PREF_CHANGED",
        source: "bar",
        value: { status: true, menu_source: "CUSTOMIZE_MENU" },
      })
    );
    assert.calledWith(DEFAULT_PROPS.setPref, "foo", true);
    wrapper.unmount();
  });

  it("should have data-eventSource attributes on relevent pref changing inputs", () => {
    wrapper = mount(<ContentSection {...DEFAULT_PROPS} />);
    assert.equal(
      wrapper.find("#shortcuts-toggle").prop("data-eventSource"),
      "TOP_SITES"
    );
    assert.equal(
      wrapper.find("#sponsored-shortcuts").prop("data-eventSource"),
      "SPONSORED_TOP_SITES"
    );
    assert.equal(
      wrapper.find("#pocket-toggle").prop("data-eventSource"),
      "TOP_STORIES"
    );
    assert.equal(
      wrapper.find("#sponsored-pocket").prop("data-eventSource"),
      "POCKET_SPOCS"
    );
    assert.equal(
      wrapper.find("#highlights-toggle").prop("data-eventSource"),
      "HIGHLIGHTS"
    );
  });
});
