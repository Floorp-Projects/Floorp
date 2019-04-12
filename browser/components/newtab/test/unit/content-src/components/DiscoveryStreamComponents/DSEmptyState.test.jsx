import {DSEmptyState} from "content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState";
import React from "react";
import {shallowWithIntl} from "test/unit/utils";

describe("<DSEmptyState>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<DSEmptyState />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".section-empty-state").exists());
  });

  it("should render message", () => {
    assert.ok(wrapper.find(".empty-state-message").exists());
    assert.ok(wrapper.find("h2").exists());
    assert.ok(wrapper.find("p").exists());
  });
});
