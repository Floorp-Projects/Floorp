import React from "react";
import { mount } from "enzyme";
import { LinkParagraph } from "content-src/components/LinkParagraph";

describe("LinkParagraph component", () => {
  let sandbox;
  let wrapper;
  let handleAction;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleAction = sandbox.stub();

    wrapper = mount(
      <LinkParagraph
        text_content={{
          text: {
            string_id:
              "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use3",
          },
          link_keys: ["privacy_policy"],
          font_styles: "legal",
        }}
        handleAction={handleAction}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render LinkParagraph component", () => {
    assert.ok(wrapper.exists());
  });

  it("should render copy with legal style if legal is passed to font_styles", () => {
    assert.strictEqual(wrapper.find(".legal-paragraph").length, 1);
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
            "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use3",
        },
        link_keys: ["privacy_policy", "terms_of_use"],
        font_styles: "legal",
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

  it("should not render LinkParagraph component if text is not passed", () => {
    wrapper.setProps({ text_content: { text: null } });
    assert.ok(wrapper.isEmptyRender());
  });

  it("should render copy in link style if no font style is passed", () => {
    wrapper.setProps({
      text_content: {
        text: {
          string_id: "shopping-onboarding-body",
        },
        link_keys: ["learn_more"],
      },
    });
    assert.strictEqual(wrapper.find(".link-paragraph").length, 1);
  });

  it("should not render links if string_id is not provided", () => {
    wrapper.setProps({
      text_content: { text: { string_id: null } },
    });
    assert.strictEqual(wrapper.find(".link-paragraph a").length, 0);
  });
});
