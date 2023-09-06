import React from "react";
import { mount } from "enzyme";
import { LegalParagraph } from "../../../content-src/aboutwelcome/components/LegalParagraph";

describe("LegalParagraph component", () => {
  let sandbox;
  let wrapper;
  let handleAction;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleAction = sandbox.stub();

    wrapper = mount(
      <LegalParagraph
        text_content={{
          text: {
            string_id:
              "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use",
          },
          link_keys: ["privacy_policy"],
        }}
        handleAction={handleAction}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render LegalParagraph component", () => {
    assert.ok(wrapper.exists());
  });

  it("should render one link when only one link id is passed", () => {
    assert.strictEqual(wrapper.find(".legal-paragraph a").length, 1);
  });

  it("should call handleAction method when link is clicked", () => {
    const linkEl = wrapper.find(".legal-paragraph a");
    linkEl.simulate("click");
    assert.calledOnce(handleAction);
  });

  it("should render two links if an additional link id is passed", () => {
    wrapper.setProps({
      text_content: {
        text: {
          string_id:
            "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use",
        },
        link_keys: ["privacy_policy", "terms_of_use"],
      },
    });
    assert.strictEqual(wrapper.find(".legal-paragraph a").length, 2);
  });

  it("should render no links when no link id is passed", () => {
    wrapper.setProps({
      text_content: { links: null },
    });
    assert.strictEqual(wrapper.find(".legal-paragraph a").length, 0);
  });

  it("should render copy even when no link id is passed", () => {
    wrapper.setProps({
      text_content: { links: null },
    });
    assert.ok(wrapper.find(".legal-paragraph"));
  });

  it("should not render LegalParagraph component if text is not passed", () => {
    wrapper.setProps({ text_content: { text: null } });
    assert.ok(wrapper.isEmptyRender());
  });

  it("should not render links if string_id is not provided", () => {
    wrapper.setProps({
      text_content: { text: { string_id: null } },
    });
    assert.strictEqual(wrapper.find(".legal-paragraph a").length, 0);
  });
});
