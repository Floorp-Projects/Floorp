import {
  Navigation,
  Topic,
} from "content-src/components/DiscoveryStreamComponents/Navigation/Navigation";
import React from "react";
import { SafeAnchor } from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";
import { shallow, mount } from "enzyme";

const DEFAULT_PROPS = {
  App: {
    isForStartupCache: false,
  },
};

describe("<Navigation>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = mount(<Navigation header={{}} />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
  });

  it("should render a title", () => {
    wrapper.setProps({ header: { title: "Foo" } });

    assert.equal(wrapper.find(".ds-header").text(), "Foo");
  });

  it("should render a FluentOrText", () => {
    wrapper.setProps({ header: { title: "Foo" } });

    assert.equal(
      wrapper
        .find(".ds-navigation")
        .children()
        .at(0)
        .type(),
      FluentOrText
    );
  });

  it("should render 2 Topics", () => {
    wrapper.setProps({
      links: [
        { url: "https://foo.com", name: "foo" },
        { url: "https://bar.com", name: "bar" },
      ],
    });

    assert.lengthOf(wrapper.find("ul").children(), 2);
  });
});

describe("<Topic>", () => {
  let wrapper;
  let sandbox;

  beforeEach(() => {
    wrapper = shallow(<Topic url="https://foo.com" name="foo" />);
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should pass onLinkClick prop", () => {
    assert.propertyVal(
      wrapper.at(0).props(),
      "onLinkClick",
      wrapper.instance().onLinkClick
    );
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.equal(wrapper.type(), SafeAnchor);
  });

  describe("onLinkClick", () => {
    let dispatch;

    beforeEach(() => {
      dispatch = sandbox.stub();
      wrapper = shallow(<Topic dispatch={dispatch} {...DEFAULT_PROPS} />);
      wrapper.setState({ isSeen: true });
    });

    it("should call dispatch", () => {
      wrapper.instance().onLinkClick({ target: { text: `Must Reads` } });

      assert.calledOnce(dispatch);
    });
  });
});
