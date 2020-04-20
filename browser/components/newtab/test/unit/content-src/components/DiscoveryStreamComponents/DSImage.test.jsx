import { DSImage } from "content-src/components/DiscoveryStreamComponents/DSImage/DSImage";
import { mount } from "enzyme";
import React from "react";

describe("Discovery Stream <DSImage>", () => {
  it("should have a child with class ds-image", () => {
    const img = mount(<DSImage />);
    const child = img.find(".ds-image");

    assert.lengthOf(child, 1);
  });

  it("should set proper sources if only `source` is available", () => {
    const img = mount(<DSImage source="https://placekitten.com/g/640/480" />);

    assert.equal(
      img.find("img").prop("src"),
      "https://placekitten.com/g/640/480"
    );
  });

  it("should set proper sources if `rawSource` is available", () => {
    const testSizes = [
      {
        mediaMatcher: "(min-width: 1122px)",
        width: 296,
        height: 148,
      },

      {
        mediaMatcher: "(min-width: 866px)",
        width: 218,
        height: 109,
      },

      {
        mediaMatcher: "(max-width: 610px)",
        width: 202,
        height: 101,
      },
    ];

    const img = mount(
      <DSImage
        rawSource="https://placekitten.com/g/640/480"
        sizes={testSizes}
      />
    );

    assert.equal(
      img.find("img").prop("src"),
      "https://placekitten.com/g/640/480"
    );
    assert.equal(
      img.find("img").prop("srcSet"),
      [
        "https://img-getpocket.cdn.mozilla.net/296x148/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 296w",
        "https://img-getpocket.cdn.mozilla.net/592x296/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 592w",
        "https://img-getpocket.cdn.mozilla.net/218x109/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 218w",
        "https://img-getpocket.cdn.mozilla.net/436x218/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 436w",
        "https://img-getpocket.cdn.mozilla.net/202x101/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 202w",
        "https://img-getpocket.cdn.mozilla.net/404x202/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fplacekitten.com%2Fg%2F640%2F480 404w",
      ].join(",")
    );
  });

  it("should fall back to unoptimized when optimized failed", () => {
    const img = mount(
      <DSImage
        source="https://placekitten.com/g/640/480"
        rawSource="https://placekitten.com/g/640/480"
      />
    );
    img.setState({
      isSeen: true,
      containerWidth: 640,
      containerHeight: 480,
    });

    img.instance().onOptimizedImageError();
    img.update();

    assert.equal(
      img.find("img").prop("src"),
      "https://placekitten.com/g/640/480"
    );
  });

  it("should render a placeholder broken image when image failed", () => {
    const img = mount(<DSImage />);
    img.setState({ isSeen: true });

    img.instance().onNonOptimizedImageError();
    img.update();

    assert.equal(img.find("div").prop("className"), "broken-image");
  });

  it("should update loaded state when seen", () => {
    const img = mount(
      <DSImage rawSource="https://placekitten.com/g/640/480" />
    );

    img.instance().onLoad();
    assert.propertyVal(img.state(), "isLoaded", true);
  });

  describe("DSImage with Idle Callback", () => {
    let wrapper;
    let windowStub = {
      requestIdleCallback: sinon.stub().returns(1),
      cancelIdleCallback: sinon.stub(),
    };
    beforeEach(() => {
      wrapper = mount(<DSImage windowObj={windowStub} />);
    });

    it("should call requestIdleCallback on componentDidMount", () => {
      assert.calledOnce(windowStub.requestIdleCallback);
    });

    it("should call cancelIdleCallback on componentWillUnmount", () => {
      wrapper.instance().componentWillUnmount();
      assert.calledOnce(windowStub.cancelIdleCallback);
    });
  });
});
