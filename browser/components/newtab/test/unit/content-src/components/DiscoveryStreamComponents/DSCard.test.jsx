import {
  _DSCard as DSCard,
  DefaultMeta,
  PlaceholderDSCard,
  CTAButtonMeta,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import {
  DSContextFooter,
  StatusMessage,
} from "content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter";
import { actionCreators as ac } from "common/Actions.jsm";
import { DSLinkMenu } from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import React from "react";
import { SafeAnchor } from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import { shallow, mount } from "enzyme";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";

const DEFAULT_PROPS = {
  App: {
    isForStartupCache: false,
  },
};

describe("<DSCard>", () => {
  let wrapper;
  let sandbox;

  beforeEach(() => {
    wrapper = shallow(<DSCard {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-card"));
  });

  it("should render a SafeAnchor", () => {
    wrapper.setProps({ url: "https://foo.com" });

    assert.equal(
      wrapper
        .children()
        .at(0)
        .type(),
      SafeAnchor
    );
    assert.propertyVal(
      wrapper
        .children()
        .at(0)
        .props(),
      "url",
      "https://foo.com"
    );
  });

  it("should pass onLinkClick prop", () => {
    assert.propertyVal(
      wrapper
        .children()
        .at(0)
        .props(),
      "onLinkClick",
      wrapper.instance().onLinkClick
    );
  });

  it("should render DSLinkMenu", () => {
    assert.equal(
      wrapper
        .children()
        .at(1)
        .type(),
      DSLinkMenu
    );
  });

  it("should start with no .active class", () => {
    assert.equal(wrapper.find(".active").length, 0);
  });

  it("should render badges for pocket, bookmark when not a spoc element ", () => {
    wrapper = mount(<DSCard context_type="bookmark" {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
    const contextFooter = wrapper.find(DSContextFooter);

    assert.lengthOf(contextFooter.find(StatusMessage), 1);
  });

  it("should render Sponsored Context for a spoc element", () => {
    const context = "Sponsored by Foo";
    wrapper = mount(
      <DSCard context_type="bookmark" context={context} {...DEFAULT_PROPS} />
    );
    wrapper.setState({ isSeen: true });
    const contextFooter = wrapper.find(DSContextFooter);

    assert.lengthOf(contextFooter.find(StatusMessage), 0);
    assert.equal(contextFooter.find(".story-sponsored-label").text(), context);
  });

  describe("onLinkClick", () => {
    let dispatch;

    beforeEach(() => {
      dispatch = sandbox.stub();
      wrapper = shallow(<DSCard dispatch={dispatch} {...DEFAULT_PROPS} />);
      wrapper.setState({ isSeen: true });
    });

    it("should call dispatch with the correct events", () => {
      wrapper.setProps({ id: "fooidx", pos: 1, type: "foo" });

      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.UserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: { card_type: "organic" },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [{ id: "fooidx", pos: 1 }],
        })
      );
    });

    it("should set the right card_type on spocs", () => {
      wrapper.setProps({ id: "fooidx", pos: 1, type: "foo", flightId: 12345 });

      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.UserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: { card_type: "spoc" },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [{ id: "fooidx", pos: 1 }],
        })
      );
    });

    it("should call dispatch with a shim", () => {
      wrapper.setProps({
        id: "fooidx",
        pos: 1,
        type: "foo",
        shim: {
          click: "click shim",
        },
      });

      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.UserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: { card_type: "organic" },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [{ id: "fooidx", pos: 1, shim: "click shim" }],
        })
      );
    });
  });

  describe("DSCard with CTA", () => {
    beforeEach(() => {
      wrapper = mount(<DSCard {...DEFAULT_PROPS} />);
      wrapper.setState({ isSeen: true });
    });

    it("should render Default Meta", () => {
      const default_meta = wrapper.find(DefaultMeta);
      assert.ok(default_meta.exists());
    });

    it("should not render cta-link for item with no cta", () => {
      const meta = wrapper.find(DefaultMeta);
      assert.notOk(meta.find(".cta-link").exists());
    });

    it("should not render cta-link by default when item has cta and cta_variant not link", () => {
      wrapper.setProps({ cta: "test" });
      const meta = wrapper.find(DefaultMeta);
      assert.notOk(meta.find(".cta-link").exists());
    });

    it("should render cta-link by default when item has cta and cta_variant as link", () => {
      wrapper.setProps({ cta: "test", cta_variant: "link" });
      const meta = wrapper.find(DefaultMeta);
      assert.equal(meta.find(".cta-link").text(), "test");
    });

    it("should not render cta-button for non spoc content", () => {
      wrapper.setProps({ cta: "test", cta_variant: "button" });
      const meta = wrapper.find(CTAButtonMeta);
      assert.lengthOf(meta.find(".cta-button"), 0);
    });

    it("should render cta-button when item has cta and cta_variant is button and is spoc", () => {
      wrapper.setProps({
        cta: "test",
        cta_variant: "button",
        context: "Sponsored by Foo",
      });
      const meta = wrapper.find(CTAButtonMeta);
      assert.equal(meta.find(".cta-button").text(), "test");
    });

    it("should not render Sponsored by label in footer for spoc item with cta_variant button", () => {
      wrapper.setProps({
        cta: "test",
        context: "Sponsored by test",
        cta_variant: "button",
      });

      assert.ok(wrapper.find(CTAButtonMeta).exists());
      assert.notOk(wrapper.find(DSContextFooter).exists());
    });

    it("should render sponsor text as fluent element on top for spoc item and cta button variant", () => {
      wrapper.setProps({
        sponsor: "Test",
        context: "Sponsored by test",
        cta_variant: "button",
      });

      assert.ok(wrapper.find(CTAButtonMeta).exists());
      const meta = wrapper.find(CTAButtonMeta);
      assert.equal(
        meta
          .find(".source")
          .children()
          .at(0)
          .type(),
        FluentOrText
      );
    });
  });
  describe("DSCard with Intersection Observer", () => {
    beforeEach(() => {
      wrapper = shallow(<DSCard {...DEFAULT_PROPS} />);
    });

    it("should render card when seen", () => {
      let card = wrapper.find("div.ds-card.placeholder");
      assert.lengthOf(card, 1);

      wrapper.instance().observer = {
        unobserve: sandbox.stub(),
      };
      wrapper.instance().placeholderElement = "element";

      wrapper.instance().onSeen([
        {
          isIntersecting: true,
        },
      ]);

      assert.isTrue(wrapper.instance().state.isSeen);
      card = wrapper.find("div.ds-card.placeholder");
      assert.lengthOf(card, 0);
      assert.lengthOf(wrapper.find(SafeAnchor), 1);
      assert.calledOnce(wrapper.instance().observer.unobserve);
      assert.calledWith(wrapper.instance().observer.unobserve, "element");
    });

    it("should setup proper placholder ref for isSeen", () => {
      wrapper.instance().setPlaceholderRef("element");
      assert.equal(wrapper.instance().placeholderElement, "element");
    });

    it("should setup observer on componentDidMount", () => {
      wrapper = mount(<DSCard {...DEFAULT_PROPS} />);
      assert.isTrue(!!wrapper.instance().observer);
    });
  });
  describe("DSCard with Idle Callback", () => {
    let windowStub = {
      requestIdleCallback: sinon.stub().returns(1),
      cancelIdleCallback: sinon.stub(),
    };
    beforeEach(() => {
      wrapper = shallow(<DSCard windowObj={windowStub} {...DEFAULT_PROPS} />);
    });

    it("should call requestIdleCallback on componentDidMount", () => {
      assert.calledOnce(windowStub.requestIdleCallback);
    });

    it("should call cancelIdleCallback on componentWillUnmount", () => {
      wrapper.instance().componentWillUnmount();
      assert.calledOnce(windowStub.cancelIdleCallback);
    });
  });
  describe("DSCard when rendered for about:home startup cache", () => {
    beforeEach(() => {
      const props = {
        App: {
          isForStartupCache: true,
        },
      };
      wrapper = mount(<DSCard {...props} />);
    });

    it("should be set as isSeen automatically", () => {
      assert.isTrue(wrapper.instance().state.isSeen);
    });
  });
});

describe("<PlaceholderDSCard> component", () => {
  it("should have placeholder prop", () => {
    const wrapper = shallow(<PlaceholderDSCard />);
    const placeholder = wrapper.prop("placeholder");
    assert.isTrue(placeholder);
  });

  it("should contain placeholder div", () => {
    const wrapper = shallow(<DSCard placeholder={true} {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
    const card = wrapper.find("div.ds-card.placeholder");
    assert.lengthOf(card, 1);
  });

  it("should not be clickable", () => {
    const wrapper = shallow(<DSCard placeholder={true} {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
    const anchor = wrapper.find("SafeAnchor.ds-card-link");
    assert.lengthOf(anchor, 0);
  });

  it("should not have context menu", () => {
    const wrapper = shallow(<DSCard placeholder={true} {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
    const linkMenu = wrapper.find(DSLinkMenu);
    assert.lengthOf(linkMenu, 0);
  });
});
