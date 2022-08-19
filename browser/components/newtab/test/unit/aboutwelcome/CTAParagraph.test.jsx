import React from "react";
import { shallow } from "enzyme";
import { CTAParagraph } from "content-src/aboutwelcome/components/CTAParagraph";

describe("CTAParagraph component", () => {
  let sandbox;
  let wrapper;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    const handleAction = sandbox.stub();
    wrapper = shallow(
      <CTAParagraph
        content={{
          text: {
            raw: "Link Text",
            string_name: "Test Name",
          },
        }}
        handleAction={handleAction}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render CTAParagraph component", () => {
    assert.ok(wrapper.exists);
  });

  it("should render CTAParagraph component if only CTA text is passed", () => {
    wrapper.setProps({ content: { text: "CTA Text" } });
    assert.ok(wrapper.exists);
  });

  it("should not render CTAParagraph component if CTA text is not passed", () => {
    wrapper.setProps({ content: { text: null } });
    assert.ok(wrapper.isEmptyRender());
  });
});
