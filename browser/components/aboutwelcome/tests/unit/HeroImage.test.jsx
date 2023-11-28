import React from "react";
import { shallow } from "enzyme";
import { HeroImage } from "content-src/components/HeroImage";

describe("HeroImage component", () => {
  const imageUrl = "https://example.com";
  const imageHeight = "100px";
  const imageAlt = "Alt text";

  let wrapper;
  beforeEach(() => {
    wrapper = shallow(
      <HeroImage url={imageUrl} alt={imageAlt} height={imageHeight} />
    );
  });

  it("should render HeroImage component", () => {
    assert.ok(wrapper.exists());
  });

  it("should render an image element with src prop", () => {
    let imgEl = wrapper.find("img");
    assert.strictEqual(imgEl.prop("src"), imageUrl);
  });

  it("should render image element with alt text prop", () => {
    let imgEl = wrapper.find("img");
    assert.equal(imgEl.prop("alt"), imageAlt);
  });

  it("should render an image with a set height prop", () => {
    let imgEl = wrapper.find("img");
    assert.propertyVal(imgEl.prop("style"), "height", imageHeight);
  });

  it("should not render HeroImage component", () => {
    wrapper.setProps({ url: null });
    assert.ok(wrapper.isEmptyRender());
  });
});
