import { Localized } from "content-src/components/MSLocalized";
import React from "react";
import { shallow } from "enzyme";

describe("<MSLocalized>", () => {
  it("should render span with no children", () => {
    const shallowWrapper = shallow(<Localized text="test" />);

    assert.ok(shallowWrapper.find("span").exists());
    assert.equal(shallowWrapper.text(), "test");
  });
  it("should render span when using string_id with no children", () => {
    const shallowWrapper = shallow(
      <Localized text={{ string_id: "test_id" }} />
    );

    assert.ok(shallowWrapper.find("span[data-l10n-id='test_id']").exists());
  });
  it("should render text inside child", () => {
    const shallowWrapper = shallow(
      <Localized text="test">
        <div />
      </Localized>
    );

    assert.ok(shallowWrapper.find("div").text(), "test");
  });
  it("should use l10n id on child", () => {
    const shallowWrapper = shallow(
      <Localized text={{ string_id: "test_id" }}>
        <div />
      </Localized>
    );

    assert.ok(shallowWrapper.find("div[data-l10n-id='test_id']").exists());
  });
  it("should keep original children", () => {
    const shallowWrapper = shallow(
      <Localized text={{ string_id: "test_id" }}>
        <h1>
          <span data-l10n-name="test" />
        </h1>
      </Localized>
    );

    assert.ok(shallowWrapper.find("span[data-l10n-name='test']").exists());
  });
});
