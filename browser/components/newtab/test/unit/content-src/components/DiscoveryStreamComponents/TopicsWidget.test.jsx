import { TopicsWidget } from "content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget";
import { shallow } from "enzyme";
import React from "react";

describe("Discovery Stream <TopicsWidget>", () => {
  let sandbox;
  let wrapper;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    wrapper = shallow(<TopicsWidget />);
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-topics-widget"));
  });
});
