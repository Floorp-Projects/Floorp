import {HorizontalRule} from "content-src/components/DiscoveryStreamComponents/HorizontalRule/HorizontalRule";
import React from "react";
import {shallowWithIntl} from "test/unit/utils";

describe("<HorizontalRule>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<HorizontalRule />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-hr").exists());
  });
});
