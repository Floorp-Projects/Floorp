import {
  _DSCard as DSCard,
  readTimeFromWordCount,
  DSSource,
  DefaultMeta,
  PlaceholderDSCard,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import {
  DSContextFooter,
  StatusMessage,
  SponsorLabel,
} from "content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter";
import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { DSLinkMenu } from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import React from "react";
import { INITIAL_STATE } from "common/Reducers.sys.mjs";
import { SafeAnchor } from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import { shallow, mount } from "enzyme";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";

const DEFAULT_PROPS = {
  url: "about:robots",
  title: "title",
  App: {
    isForStartupCache: false,
  },
  DiscoveryStream: INITIAL_STATE.DiscoveryStream,
};

describe("<DSCard>", () => {
  let wrapper;
  let sandbox;
  let dispatch;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    wrapper = shallow(<DSCard dispatch={dispatch} {...DEFAULT_PROPS} />);
    wrapper.setState({ isSeen: true });
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

    assert.equal(wrapper.children().at(0).type(), SafeAnchor);
    assert.propertyVal(
      wrapper.children().at(0).props(),
      "url",
      "https://foo.com"
    );
  });

  it("should pass onLinkClick prop", () => {
    assert.propertyVal(
      wrapper.children().at(0).props(),
      "onLinkClick",
      wrapper.instance().onLinkClick
    );
  });

  it("should render DSLinkMenu", () => {
    assert.equal(wrapper.children().at(1).type(), DSLinkMenu);
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

  it("should render time to read", () => {
    const discoveryStream = {
      ...INITIAL_STATE.DiscoveryStream,
      readTime: true,
    };
    wrapper = mount(
      <DSCard
        time_to_read={4}
        {...DEFAULT_PROPS}
        DiscoveryStream={discoveryStream}
      />
    );
    wrapper.setState({ isSeen: true });
    const defaultMeta = wrapper.find(DefaultMeta);
    assert.lengthOf(defaultMeta, 1);
    assert.equal(defaultMeta.props().timeToRead, 4);
  });

  it("should not show save to pocket button for spocs", () => {
    wrapper.setProps({
      id: "fooidx",
      pos: 1,
      type: "foo",
      flightId: 12345,
      saveToPocketCard: true,
    });

    let stpButton = wrapper.find(".card-stp-button");

    assert.lengthOf(stpButton, 0);
  });

  it("should show save to pocket button for non-spocs", () => {
    wrapper.setProps({
      id: "fooidx",
      pos: 1,
      type: "foo",
      saveToPocketCard: true,
    });

    let stpButton = wrapper.find(".card-stp-button");

    assert.lengthOf(stpButton, 1);
  });

  describe("onLinkClick", () => {
    let fakeWindow;

    beforeEach(() => {
      fakeWindow = {
        requestIdleCallback: sinon.stub().returns(1),
        cancelIdleCallback: sinon.stub(),
        innerWidth: 1000,
        innerHeight: 900,
      };
      wrapper = mount(
        <DSCard {...DEFAULT_PROPS} dispatch={dispatch} windowObj={fakeWindow} />
      );
    });

    it("should call dispatch with the correct events", () => {
      wrapper.setProps({ id: "fooidx", pos: 1, type: "foo" });

      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: {
            card_type: "organic",
            recommendation_id: undefined,
            tile_id: "fooidx",
          },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [
            {
              id: "fooidx",
              pos: 1,
              type: "organic",
              recommendation_id: undefined,
            },
          ],
          window_inner_width: 1000,
          window_inner_height: 900,
        })
      );
    });

    it("should set the right card_type on spocs", () => {
      wrapper.setProps({ id: "fooidx", pos: 1, type: "foo", flightId: 12345 });

      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: {
            card_type: "spoc",
            recommendation_id: undefined,
            tile_id: "fooidx",
          },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [
            {
              id: "fooidx",
              pos: 1,
              type: "spoc",
              recommendation_id: undefined,
            },
          ],
          window_inner_width: 1000,
          window_inner_height: 900,
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
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
          value: {
            card_type: "organic",
            recommendation_id: undefined,
            tile_id: "fooidx",
            shim: "click shim",
          },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [
            {
              id: "fooidx",
              pos: 1,
              shim: "click shim",
              type: "organic",
              recommendation_id: undefined,
            },
          ],
          window_inner_width: 1000,
          window_inner_height: 900,
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
        DiscoveryStream: INITIAL_STATE.DiscoveryStream,
      };
      wrapper = mount(<DSCard {...props} />);
    });

    it("should be set as isSeen automatically", () => {
      assert.isTrue(wrapper.instance().state.isSeen);
    });
  });

  describe("DSCard onSaveClick", () => {
    it("should fire telemetry for onSaveClick", () => {
      wrapper.setProps({ id: "fooidx", pos: 1, type: "foo" });
      wrapper.instance().onSaveClick();

      assert.calledThrice(dispatch);
      assert.calledWith(
        dispatch,
        ac.AlsoToMain({
          type: at.SAVE_TO_POCKET,
          data: { site: { url: "about:robots", title: "title" } },
        })
      );
      assert.calledWith(
        dispatch,
        ac.DiscoveryStreamUserEvent({
          event: "SAVE_TO_POCKET",
          source: "CARDGRID_HOVER",
          action_position: 1,
          value: {
            card_type: "organic",
            recommendation_id: undefined,
            tile_id: "fooidx",
          },
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          source: "CARDGRID_HOVER",
          pocket: 0,
          tiles: [
            {
              id: "fooidx",
              pos: 1,
              recommendation_id: undefined,
            },
          ],
        })
      );
    });
  });

  describe("DSCard menu open states", () => {
    let cardNode;
    let fakeDocument;
    let fakeWindow;

    beforeEach(() => {
      fakeDocument = { l10n: { translateFragment: sinon.stub() } };
      fakeWindow = {
        document: fakeDocument,
        requestIdleCallback: sinon.stub().returns(1),
        cancelIdleCallback: sinon.stub(),
      };
      wrapper = mount(<DSCard {...DEFAULT_PROPS} windowObj={fakeWindow} />);
      wrapper.setState({ isSeen: true });
      cardNode = wrapper.getDOMNode();
    });

    it("Should remove active on Menu Update", () => {
      // Add active class name to DSCard wrapper
      // to simulate menu open state
      cardNode.classList.add("active");
      assert.equal(
        cardNode.className,
        "ds-card ds-card-title-lines-3 ds-card-desc-lines-3 active"
      );

      wrapper.instance().onMenuUpdate(false);
      wrapper.update();

      assert.equal(
        cardNode.className,
        "ds-card ds-card-title-lines-3 ds-card-desc-lines-3"
      );
    });

    it("Should add active on Menu Show", async () => {
      await wrapper.instance().onMenuShow();
      wrapper.update();
      assert.equal(
        cardNode.className,
        "ds-card ds-card-title-lines-3 ds-card-desc-lines-3 active"
      );
    });

    it("Should add last-item to support resized window", async () => {
      fakeWindow.scrollMaxX = 20;
      await wrapper.instance().onMenuShow();
      wrapper.update();
      assert.equal(
        cardNode.className,
        "ds-card ds-card-title-lines-3 ds-card-desc-lines-3 last-item active"
      );
    });

    it("should remove .active and .last-item classes", () => {
      const instance = wrapper.instance();
      const remove = sinon.stub();
      instance.contextMenuButtonHostElement = {
        classList: { remove },
      };
      instance.onMenuUpdate();
      assert.calledOnce(remove);
    });

    it("should add .active and .last-item classes", async () => {
      const instance = wrapper.instance();
      const add = sinon.stub();
      instance.contextMenuButtonHostElement = {
        classList: { add },
      };
      await instance.onMenuShow();
      assert.calledOnce(add);
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

describe("<DSSource> component", () => {
  it("should return a default source without compact", () => {
    const wrapper = shallow(<DSSource source="Mozilla" />);

    let sourceElement = wrapper.find(".source");
    assert.equal(sourceElement.text(), "Mozilla");
  });
  it("should return a default source with compact without a sponsor or time to read", () => {
    const wrapper = shallow(<DSSource compact={true} source="Mozilla" />);

    let sourceElement = wrapper.find(".source");
    assert.equal(sourceElement.text(), "Mozilla");
  });
  it("should return a SponsorLabel with compact and a sponsor", () => {
    const wrapper = shallow(
      <DSSource newSponsoredLabel={true} sponsor="Mozilla" />
    );
    const sponsorLabel = wrapper.find(SponsorLabel);
    assert.lengthOf(sponsorLabel, 1);
  });
  it("should return a time to read with compact and without a sponsor but with a time to read", () => {
    const wrapper = shallow(
      <DSSource compact={true} source="Mozilla" timeToRead="2000" />
    );

    let timeToRead = wrapper.find(".time-to-read");
    assert.lengthOf(timeToRead, 1);

    // Weirdly, we can test for the pressence of fluent, because time to read needs to be translated.
    // This is also because we did a shallow render, that th contents of fluent would be empty anyway.
    const fluentOrText = wrapper.find(FluentOrText);
    assert.lengthOf(fluentOrText, 1);
  });
  it("should prioritize a SponsorLabel if for some reason it gets everything", () => {
    const wrapper = shallow(
      <DSSource
        newSponsoredLabel={true}
        sponsor="Mozilla"
        source="Mozilla"
        timeToRead="2000"
      />
    );
    const sponsorLabel = wrapper.find(SponsorLabel);
    assert.lengthOf(sponsorLabel, 1);
  });
});

describe("readTimeFromWordCount function", () => {
  it("should return proper read time", () => {
    const result = readTimeFromWordCount(2000);
    assert.equal(result, 10);
  });
  it("should return false with falsey word count", () => {
    assert.isFalse(readTimeFromWordCount());
    assert.isFalse(readTimeFromWordCount(0));
    assert.isFalse(readTimeFromWordCount(""));
    assert.isFalse(readTimeFromWordCount(null));
    assert.isFalse(readTimeFromWordCount(undefined));
  });
  it("should return NaN with invalid word count", () => {
    assert.isNaN(readTimeFromWordCount("zero"));
    assert.isNaN(readTimeFromWordCount({}));
  });
});
