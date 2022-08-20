import React from "react";
import { shallow } from "enzyme";
import { LinkText } from "content-src/aboutwelcome/components/LinkText";

describe("LinkText component", () => {
  let sandbox;
  let wrapper;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    const handleAction = sandbox.stub();
    wrapper = shallow(
      <LinkText
        content={{ text: "Link Text", button_label: "Button Label" }}
        handleAction={handleAction}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render LinkText component", () => {
    assert.ok(wrapper.exists);
  });

  it("should not render LinkText component if button text is not passed", () => {
    wrapper.setProps({ content: { text: null, button_label: "Button Label" } });
    assert.ok(wrapper.isEmptyRender());
  });

  it("should not render LinkText component if button action is not passed", () => {
    wrapper.setProps({ handleAction: null });
    assert.ok(wrapper.isEmptyRender());
  });
});
