import React from "react";
import { shallow } from "enzyme";
import { LegalParagraph } from "../../../content-src/aboutwelcome/components/LegalParagraph";

describe("LegalParagraph component", () => {
  let sandbox;
  let wrapper;
  let handleAction;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleAction = sandbox.stub();

    wrapper = shallow(
      <LegalParagraph
        content={{
          legal_paragraph: {
            text: {
              string_id:
                "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use",
            },
            link_keys: ["privacy_policy"],
          },
          privacy_policy: {
            action: {
              type: "OPEN_URL",
              data: {
                args: "https://www.mozilla.org/privacy/firefox/",
                where: "tab",
              },
            },
          },
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

  it("should render two links if an additional link id is passed", () => {
    wrapper.setProps({
      content: {
        legal_paragraph: {
          text: {
            string_id:
              "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use",
          },
          link_keys: ["privacy_policy", "terms_of_use"],
        },
        privacy_policy: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/privacy/firefox/",
              where: "tab",
            },
          },
        },
        temrs_of_use: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/about/legal/terms/firefox/",
              where: "tab",
            },
          },
        },
      },
    });
    assert.strictEqual(wrapper.find("a").length, 2);
  });

  it("should render no links when no link id is passed", () => {
    wrapper.setProps({
      content: {
        legal_paragraph: { links: null },
      },
    });
    assert.strictEqual(wrapper.find("a").length, 0);
  });

  it("should not render LegalParagraph component if text is not passed", () => {
    wrapper.setProps({ content: { legal_paragraph: { text: null } } });
    assert.ok(wrapper.isEmptyRender());
  });
});
