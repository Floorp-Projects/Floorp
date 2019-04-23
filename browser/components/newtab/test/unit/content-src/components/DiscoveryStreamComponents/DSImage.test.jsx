import {DSImage} from "content-src/components/DiscoveryStreamComponents/DSImage/DSImage";
import {mount} from "enzyme";
import React from "react";

describe("Discovery Stream <DSImage>", () => {
  it("should have a child with class ds-image", () => {
    const img = mount(<DSImage />);
    const child = img.find(".ds-image");

    assert.lengthOf(child, 1);
  });

  it("should set proper sources if only `source` is available", () => {
    const img = mount(<DSImage source="https://placekitten.com/g/640/480" />);

    img.setState({
      isSeen: true,
      containerWidth: 640,
    });

    assert.equal(img.find("img").prop("src"), "https://placekitten.com/g/640/480");
  });

  it("should set proper sources if `rawSource` is available", () => {
    const img = mount(<DSImage rawSource="https://placekitten.com/g/640/480" />);

    img.setState({
      isSeen: true,
      containerWidth: 640,
      containerHeight: 480,
    });

    assert.equal(img.find("img").prop("src"), "https://img-getpocket.cdn.mozilla.net/640x480/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480");
    assert.equal(img.find("img").prop("srcSet"), "https://img-getpocket.cdn.mozilla.net/1280x960/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 2x");
  });
});
