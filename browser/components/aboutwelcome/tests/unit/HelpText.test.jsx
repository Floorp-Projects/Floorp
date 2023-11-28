import { HelpText } from "content-src/components/HelpText";
import { Localized } from "content-src/components/MSLocalized";
import React from "react";
import { shallow } from "enzyme";

describe("<HelpText>", () => {
  it("should render text inside Localized", () => {
    const shallowWrapper = shallow(<HelpText text="test" />);

    assert.equal(shallowWrapper.find(Localized).props().text, "test");
  });
  it("should render the img if there is an img and a string_id", () => {
    const shallowWrapper = shallow(
      <HelpText
        text={{ string_id: "test_id" }}
        hasImg={{
          src: "chrome://activity-stream/content/data/content/assets/cfr_fb_container.png",
        }}
      />
    );
    assert.ok(
      shallowWrapper
        .find(Localized)
        .findWhere(n => n.text.string_id === "test_id")
    );
    assert.lengthOf(shallowWrapper.find("p.helptext"), 1);
    assert.lengthOf(shallowWrapper.find("img[data-l10n-name='help-img']"), 1);
  });
  it("should render the img if there is an img and plain text", () => {
    const shallowWrapper = shallow(
      <HelpText
        text={"Sample help text"}
        hasImg={{
          src: "chrome://activity-stream/content/data/content/assets/cfr_fb_container.png",
        }}
      />
    );
    assert.equal(shallowWrapper.find("p.helptext").text(), "Sample help text");
    assert.lengthOf(shallowWrapper.find("img.helptext-img"), 1);
  });
});
