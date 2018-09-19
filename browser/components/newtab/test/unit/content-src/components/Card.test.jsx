import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {_Card as Card, PlaceholderCard} from "content-src/components/Card/Card";
import {combineReducers, createStore} from "redux";
import {GlobalOverrider, mountWithIntl} from "test/unit/utils";
import {INITIAL_STATE, reducers} from "common/Reducers.jsm";
import {cardContextTypes} from "content-src/components/Card/types";
import {LinkMenu} from "content-src/components/LinkMenu/LinkMenu";
import {Provider} from "react-redux";
import React from "react";
import {shallow} from "enzyme";

let DEFAULT_PROPS = {
  dispatch: sinon.stub(),
  index: 0,
  link: {
    hostname: "foo",
    title: "A title for foo",
    url: "http://www.foo.com",
    type: "history",
    description: "A description for foo",
    image: "http://www.foo.com/img.png",
    guid: 1
  },
  eventSource: "TOP_STORIES",
  shouldSendImpressionStats: true,
  contextMenuOptions: ["Separator"]
};

let DEFAULT_BLOB_IMAGE = {
  path: "/testpath",
  data: new Blob([0])
};

function mountCardWithProps(props) {
  const store = createStore(combineReducers(reducers), INITIAL_STATE);
  return mountWithIntl(<Provider store={store}><Card {...props} /></Provider>);
}

describe("<Card>", () => {
  let globals;
  let wrapper;
  beforeEach(() => {
    globals = new GlobalOverrider();
    wrapper = mountCardWithProps(DEFAULT_PROPS);
  });
  afterEach(() => {
    DEFAULT_PROPS.dispatch.reset();
    globals.restore();
  });
  it("should render a Card component", () => assert.ok(wrapper.exists()));
  it("should add the right url", () => {
    assert.propertyVal(wrapper.find("a").props(), "href", DEFAULT_PROPS.link.url);

    // test that pocket cards get a special open_url href
    const pocketLink = Object.assign({}, DEFAULT_PROPS.link, {open_url: "getpocket.com/foo", type: "pocket"});
    wrapper = mountWithIntl(<Card {...Object.assign({}, DEFAULT_PROPS, {link: pocketLink})} />);
    assert.propertyVal(wrapper.find("a").props(), "href", pocketLink.open_url);
  });
  it("should display a title", () => assert.equal(wrapper.find(".card-title").text(), DEFAULT_PROPS.link.title));
  it("should display a description", () => (
    assert.equal(wrapper.find(".card-description").text(), DEFAULT_PROPS.link.description)
  ));
  it("should display a host name", () => assert.equal(wrapper.find(".card-host-name").text(), "foo"));
  it("should have a link menu button", () => assert.ok(wrapper.find(".context-menu-button").exists()));
  it("should render a link menu when button is clicked", () => {
    const button = wrapper.find(".context-menu-button");
    assert.equal(wrapper.find(LinkMenu).length, 0);
    button.simulate("click", {preventDefault: () => {}});
    assert.equal(wrapper.find(LinkMenu).length, 1);
  });
  it("should pass dispatch, source, onUpdate, site, options, and index to LinkMenu", () => {
    wrapper.find(".context-menu-button").simulate("click", {preventDefault: () => {}});
    const {dispatch, source, onUpdate, site, options, index} = wrapper.find(LinkMenu).props();
    assert.equal(dispatch, DEFAULT_PROPS.dispatch);
    assert.equal(source, DEFAULT_PROPS.eventSource);
    assert.ok(onUpdate);
    assert.equal(site, DEFAULT_PROPS.link);
    assert.equal(options, DEFAULT_PROPS.contextMenuOptions);
    assert.equal(index, DEFAULT_PROPS.index);
  });
  it("should pass through the correct menu options to LinkMenu if overridden by individual card", () => {
    const link = Object.assign({}, DEFAULT_PROPS.link);
    link.contextMenuOptions = ["CheckBookmark"];

    wrapper = mountCardWithProps(Object.assign({}, DEFAULT_PROPS, {link}));
    wrapper.find(".context-menu-button").simulate("click", {preventDefault: () => {}});
    const {options} = wrapper.find(LinkMenu).props();
    assert.equal(options, link.contextMenuOptions);
  });
  it("should have a context based on type", () => {
    wrapper = shallow(<Card {...DEFAULT_PROPS} />);
    const context = wrapper.find(".card-context");
    const {icon, intlID} = cardContextTypes[DEFAULT_PROPS.link.type];
    assert.isTrue(context.childAt(0).hasClass(`icon-${icon}`));
    assert.isTrue(context.childAt(1).hasClass("card-context-label"));
    assert.equal(context.childAt(1).props().children.props.id, intlID);
  });
  it("should support setting custom context", () => {
    const linkWithCustomContext = {
      type: "history",
      context: "Custom",
      icon: "icon-url"
    };

    wrapper = shallow(<Card {...Object.assign({}, DEFAULT_PROPS, {link: linkWithCustomContext})} />);
    const context = wrapper.find(".card-context");
    const {icon} = cardContextTypes[DEFAULT_PROPS.link.type];
    assert.isFalse(context.childAt(0).hasClass(`icon-${icon}`));
    assert.equal(context.childAt(0).props().style.backgroundImage, "url('icon-url')");

    assert.isTrue(context.childAt(1).hasClass("card-context-label"));
    assert.equal(context.childAt(1).text(), linkWithCustomContext.context);
  });
  it("should have .active class, on card-outer if context menu is open", () => {
    const button = wrapper.find(".context-menu-button");
    assert.isFalse(wrapper.find(".card-outer").hasClass("active"));
    button.simulate("click", {preventDefault: () => {}});
    assert.isTrue(wrapper.find(".card-outer").hasClass("active"));
  });
  it("should send SHOW_DOWNLOAD_FILE if we clicked on a download", () => {
    const downloadLink = {
      type: "download",
      url: "download.mov"
    };
    wrapper = mountCardWithProps(Object.assign({}, DEFAULT_PROPS, {link: downloadLink}));
    const card = wrapper.find(".card");
    card.simulate("click", {preventDefault: () => {}});
    assert.calledThrice(DEFAULT_PROPS.dispatch);

    assert.equal(DEFAULT_PROPS.dispatch.firstCall.args[0].type, at.SHOW_DOWNLOAD_FILE);
    assert.deepEqual(DEFAULT_PROPS.dispatch.firstCall.args[0].data, downloadLink);
  });
  it("should send OPEN_LINK if we clicked on anything other than a download", () => {
    const nonDownloadLink = {
      type: "history",
      url: "download.mov"
    };
    wrapper = mountCardWithProps(Object.assign({}, DEFAULT_PROPS, {link: nonDownloadLink}));
    const card = wrapper.find(".card");
    const event = {altKey: "1", button: "2", ctrlKey: "3", metaKey: "4", shiftKey: "5"};
    card.simulate("click", Object.assign({}, event, {preventDefault: () => {}}));
    assert.calledThrice(DEFAULT_PROPS.dispatch);

    assert.equal(DEFAULT_PROPS.dispatch.firstCall.args[0].type, at.OPEN_LINK);
  });
  describe("card image display", () => {
    const DEFAULT_BLOB_URL = "blob://test";
    let url;
    beforeEach(() => {
      url = {
        createObjectURL: globals.sandbox.stub().returns(DEFAULT_BLOB_URL),
        revokeObjectURL: globals.sandbox.spy()
      };
      globals.set("URL", url);
    });
    afterEach(() => {
      globals.restore();
    });
    it("should display a regular image correctly and not call revokeObjectURL when unmounted", () => {
      wrapper = shallow(<Card {...DEFAULT_PROPS} />);

      assert.isUndefined(wrapper.state("cardImage").path);
      assert.equal(wrapper.state("cardImage").url, DEFAULT_PROPS.link.image);
      assert.equal(wrapper.find(".card-preview-image").props().style.backgroundImage, `url(${wrapper.state("cardImage").url})`);

      wrapper.unmount();
      assert.notCalled(url.revokeObjectURL);
    });
    it("should display a blob image correctly and revoke blob url when unmounted", () => {
      const link = Object.assign({}, DEFAULT_PROPS.link, {image: DEFAULT_BLOB_IMAGE});
      wrapper = shallow(<Card {...DEFAULT_PROPS} link={link} />);

      assert.equal(wrapper.state("cardImage").path, DEFAULT_BLOB_IMAGE.path);
      assert.equal(wrapper.state("cardImage").url, DEFAULT_BLOB_URL);
      assert.equal(wrapper.find(".card-preview-image").props().style.backgroundImage, `url(${wrapper.state("cardImage").url})`);

      wrapper.unmount();
      assert.calledOnce(url.revokeObjectURL);
    });
    it("should not show an image if there isn't one and not call revokeObjectURL when unmounted", () => {
      const link = Object.assign({}, DEFAULT_PROPS.link);
      delete link.image;

      wrapper = shallow(<Card {...DEFAULT_PROPS} link={link} />);

      assert.isNull(wrapper.state("cardImage"));
      assert.lengthOf(wrapper.find(".card-preview-image"), 0);

      wrapper.unmount();
      assert.notCalled(url.revokeObjectURL);
    });
    it("should remove current card image if new image is not present", () => {
      wrapper = shallow(<Card {...DEFAULT_PROPS} />);

      const otherLink = Object.assign({}, DEFAULT_PROPS.link);
      delete otherLink.image;
      wrapper.setProps(Object.assign({}, DEFAULT_PROPS, {link: otherLink}));

      assert.isNull(wrapper.state("cardImage"));
    });
    it("should not create or revoke urls if normal image is already in state", () => {
      wrapper = shallow(<Card {...DEFAULT_PROPS} />);

      wrapper.setProps(DEFAULT_PROPS);

      assert.notCalled(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);
    });
    it("should not create or revoke more urls if blob image is already in state", () => {
      const link = Object.assign({}, DEFAULT_PROPS.link, {image: DEFAULT_BLOB_IMAGE});
      wrapper = shallow(<Card {...DEFAULT_PROPS} link={link} />);

      assert.calledOnce(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);

      wrapper.setProps(Object.assign({}, DEFAULT_PROPS, {link}));

      assert.calledOnce(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);
    });
    it("should create blob urls for new blobs and revoke existing ones", () => {
      const link = Object.assign({}, DEFAULT_PROPS.link, {image: DEFAULT_BLOB_IMAGE});
      wrapper = shallow(<Card {...DEFAULT_PROPS} link={link} />);

      assert.calledOnce(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);

      const otherLink = Object.assign({}, DEFAULT_PROPS.link, {image: {path: "/newpath", data: new Blob([0])}});
      wrapper.setProps(Object.assign({}, DEFAULT_PROPS, {link: otherLink}));

      assert.calledTwice(url.createObjectURL);
      assert.calledOnce(url.revokeObjectURL);
    });
    it("should not call createObjectURL and revokeObjectURL for normal images", () => {
      wrapper = shallow(<Card {...DEFAULT_PROPS} />);

      assert.notCalled(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);

      const otherLink = Object.assign({}, DEFAULT_PROPS.link, {image: "https://other/image"});
      wrapper.setProps(Object.assign({}, DEFAULT_PROPS, {link: otherLink}));

      assert.notCalled(url.createObjectURL);
      assert.notCalled(url.revokeObjectURL);
    });
  });
  describe("image loading", () => {
    let link;
    let triggerImage = {};
    let uniqueLink = 0;
    beforeEach(() => {
      global.Image.prototype = {
        addEventListener(event, callback) {
          triggerImage[event] = () => Promise.resolve(callback());
        }
      };

      link = Object.assign({}, DEFAULT_PROPS.link);
      link.image += uniqueLink++;
      wrapper = shallow(<Card {...DEFAULT_PROPS} link={link} />);
    });
    it("should have a loaded preview image when the image is loaded", () => {
      assert.isFalse(wrapper.find(".card-preview-image").hasClass("loaded"));

      wrapper.setState({imageLoaded: true});

      assert.isTrue(wrapper.find(".card-preview-image").hasClass("loaded"));
    });
    it("should start not loaded", () => {
      assert.isFalse(wrapper.state("imageLoaded"));
    });
    it("should be loaded after load", async () => {
      await triggerImage.load();

      assert.isTrue(wrapper.state("imageLoaded"));
    });
    it("should be not be loaded after error ", async () => {
      await triggerImage.error();

      assert.isFalse(wrapper.state("imageLoaded"));
    });
    it("should be not be loaded if image changes", async () => {
      await triggerImage.load();
      const otherLink = Object.assign({}, link, {image: "https://other/image"});

      wrapper.setProps(Object.assign({}, DEFAULT_PROPS, {link: otherLink}));

      assert.isFalse(wrapper.state("imageLoaded"));
    });
  });
  describe("placeholder=true", () => {
    beforeEach(() => {
      wrapper = mountWithIntl(<Card placeholder={true} />);
    });
    it("should render when placeholder=true", () => {
      assert.ok(wrapper.exists());
    });
    it("should add a placeholder class to the outer element", () => {
      assert.isTrue(wrapper.find(".card-outer").hasClass("placeholder"));
    });
    it("should not have a context menu button or LinkMenu", () => {
      assert.isFalse(wrapper.find(".context-menu-button").exists(), "context menu button");
      assert.isFalse(wrapper.find(LinkMenu).exists(), "LinkMenu");
    });
    it("should not call onLinkClick when the link is clicked", () => {
      const spy = sinon.spy(wrapper.instance(), "onLinkClick");
      const card = wrapper.find(".card");
      card.simulate("click");
      assert.notCalled(spy);
    });
  });
  describe("#trackClick", () => {
    it("should call dispatch when the link is clicked with the right data", () => {
      const card = wrapper.find(".card");
      const event = {altKey: "1", button: "2", ctrlKey: "3", metaKey: "4", shiftKey: "5"};
      card.simulate("click", Object.assign({}, event, {preventDefault: () => {}}));
      assert.calledThrice(DEFAULT_PROPS.dispatch);

      // first dispatch call is the AlsoToMain message which will open a link in a window, and send some event data
      assert.equal(DEFAULT_PROPS.dispatch.firstCall.args[0].type, at.OPEN_LINK);
      assert.deepEqual(DEFAULT_PROPS.dispatch.firstCall.args[0].data.event, event);

      // second dispatch call is a UserEvent action for telemetry
      assert.isUserEventAction(DEFAULT_PROPS.dispatch.secondCall.args[0]);
      assert.calledWith(DEFAULT_PROPS.dispatch.secondCall, ac.UserEvent({
        event: "CLICK",
        source: DEFAULT_PROPS.eventSource,
        action_position: DEFAULT_PROPS.index
      }));

      // third dispatch call is to send impression stats
      assert.calledWith(DEFAULT_PROPS.dispatch.thirdCall, ac.ImpressionStats({
        source: DEFAULT_PROPS.eventSource,
        click: 0,
        tiles: [{id: DEFAULT_PROPS.link.guid, pos: DEFAULT_PROPS.index}]
      }));
    });
    it("should provide card_type to telemetry info if type is not history", () => {
      const link = Object.assign({}, DEFAULT_PROPS.link);
      link.type = "bookmark";
      wrapper = mountWithIntl(<Card {...Object.assign({}, DEFAULT_PROPS, {link})} />);
      const card = wrapper.find(".card");
      const event = {altKey: "1", button: "2", ctrlKey: "3", metaKey: "4", shiftKey: "5"};

      card.simulate("click", Object.assign({}, event, {preventDefault: () => {}}));

      assert.isUserEventAction(DEFAULT_PROPS.dispatch.secondCall.args[0]);
      assert.calledWith(DEFAULT_PROPS.dispatch.secondCall, ac.UserEvent({
        event: "CLICK",
        source: DEFAULT_PROPS.eventSource,
        action_position: DEFAULT_PROPS.index,
        value: {card_type: link.type}
      }));
    });
    it("should notify Web Extensions with WEBEXT_CLICK if props.isWebExtension is true", () => {
      wrapper = mountCardWithProps(Object.assign({}, DEFAULT_PROPS, {isWebExtension: true, eventSource: "MyExtension", index: 3}));
      const card = wrapper.find(".card");
      const event = {preventDefault() {}};
      card.simulate("click", event);
      assert.calledWith(DEFAULT_PROPS.dispatch, ac.WebExtEvent(at.WEBEXT_CLICK, {
        source: "MyExtension",
        url: DEFAULT_PROPS.link.url,
        action_position: 3
      }));
    });
  });
});

describe("<PlaceholderCard />", () => {
  it("should render a Card with placeholder=true", () => {
    const wrapper = mountWithIntl(<Provider store={createStore(combineReducers(reducers), INITIAL_STATE)}><PlaceholderCard /></Provider>);
    assert.isTrue(wrapper.find(Card).props().placeholder);
  });
});
