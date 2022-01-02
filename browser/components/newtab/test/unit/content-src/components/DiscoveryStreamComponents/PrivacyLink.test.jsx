import { PrivacyLink } from "content-src/components/DiscoveryStreamComponents/PrivacyLink/PrivacyLink";
import React from "react";
import { shallow } from "enzyme";

describe("<PrivacyLink>", () => {
  let wrapper;
  let sandbox;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    wrapper = shallow(
      <PrivacyLink
        properties={{
          url: "url",
          title: "Privacy Link",
        }}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-privacy-link").exists());
  });
});
