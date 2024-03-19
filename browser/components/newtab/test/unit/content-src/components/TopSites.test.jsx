import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { GlobalOverrider } from "test/unit/utils";
import { MIN_RICH_FAVICON_SIZE } from "content-src/components/TopSites/TopSitesConstants";
import {
  TOP_SITES_DEFAULT_ROWS,
  TOP_SITES_MAX_SITES_PER_ROW,
} from "common/Reducers.sys.mjs";
import {
  TopSite,
  TopSiteLink,
  _TopSiteList as TopSiteList,
  TopSitePlaceholder,
} from "content-src/components/TopSites/TopSite";
import {
  INTERSECTION_RATIO,
  TopSiteImpressionWrapper,
} from "content-src/components/TopSites/TopSiteImpressionWrapper";
import { A11yLinkButton } from "content-src/components/A11yLinkButton/A11yLinkButton";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";
import { mount, shallow } from "enzyme";
import { TopSiteForm } from "content-src/components/TopSites/TopSiteForm";
import { TopSiteFormInput } from "content-src/components/TopSites/TopSiteFormInput";
import { _TopSites as TopSites } from "content-src/components/TopSites/TopSites";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";

const perfSvc = {
  mark() {},
  getMostRecentAbsMarkStartByName() {},
};

const DEFAULT_PROPS = {
  Prefs: { values: { featureConfig: {} } },
  TopSites: { initialized: true, rows: [] },
  TopSitesRows: TOP_SITES_DEFAULT_ROWS,
  topSiteIconType: () => "no_image",
  dispatch() {},
  perfSvc,
};

const DEFAULT_BLOB_URL = "blob://test";

describe("<TopSites>", () => {
  let sandbox;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render a TopSites element", () => {
    const wrapper = shallow(<TopSites {...DEFAULT_PROPS} />);
    assert.ok(wrapper.exists());
  });
  describe("#_dispatchTopSitesStats", () => {
    let globals;
    let wrapper;
    let dispatchStatsSpy;

    beforeEach(() => {
      globals = new GlobalOverrider();
      sandbox.stub(DEFAULT_PROPS, "dispatch");
      wrapper = shallow(<TopSites {...DEFAULT_PROPS} />, {
        disableLifecycleMethods: true,
      });
      dispatchStatsSpy = sandbox.spy(
        wrapper.instance(),
        "_dispatchTopSitesStats"
      );
    });
    afterEach(() => {
      globals.restore();
      sandbox.restore();
    });
    it("should call _dispatchTopSitesStats on componentDidMount", () => {
      wrapper.instance().componentDidMount();

      assert.calledOnce(dispatchStatsSpy);
    });
    it("should call _dispatchTopSitesStats on componentDidUpdate", () => {
      wrapper.instance().componentDidUpdate();

      assert.calledOnce(dispatchStatsSpy);
    });
    it("should dispatch SAVE_SESSION_PERF_DATA", () => {
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 0,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count TopSite images - just screenshot", () => {
      const rows = [{ screenshot: true }];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 1,
              tippytop: 0,
              rich_icon: 0,
              no_image: 0,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count TopSite images - custom_screenshot", () => {
      const rows = [{ customScreenshotURL: true }];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 1,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 0,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count TopSite images - rich_icon", () => {
      const rows = [{ faviconSize: MIN_RICH_FAVICON_SIZE }];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 1,
              no_image: 0,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count TopSite images - tippytop", () => {
      const rows = [
        { tippyTopIcon: "foo" },
        { faviconRef: "tippytop" },
        { faviconRef: "foobar" },
      ];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 2,
              rich_icon: 0,
              no_image: 1,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count TopSite images - no image", () => {
      const rows = [{}];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 1,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count pinned Top Sites", () => {
      const rows = [
        { isPinned: true },
        { isPinned: false },
        { isPinned: true },
      ];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 3,
            },
            topsites_pinned: 2,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should correctly count search shortcut Top Sites", () => {
      const rows = [{ searchTopSite: true }, { searchTopSite: true }];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();

      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 2,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 2,
          },
        })
      );
    });
    it("should only count visible top sites on wide layout", () => {
      globals.set("matchMedia", () => ({ matches: true }));
      const rows = [
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
      ];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);

      wrapper.instance()._dispatchTopSitesStats();
      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 8,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
    it("should only count visible top sites on normal layout", () => {
      globals.set("matchMedia", () => ({ matches: false }));
      const rows = [
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        {},
      ];
      sandbox.stub(DEFAULT_PROPS.TopSites, "rows").value(rows);
      wrapper.instance()._dispatchTopSitesStats();
      assert.calledOnce(DEFAULT_PROPS.dispatch);
      assert.calledWithExactly(
        DEFAULT_PROPS.dispatch,
        ac.AlsoToMain({
          type: at.SAVE_SESSION_PERF_DATA,
          data: {
            topsites_icon_stats: {
              custom_screenshot: 0,
              screenshot: 0,
              tippytop: 0,
              rich_icon: 0,
              no_image: 6,
            },
            topsites_pinned: 0,
            topsites_search_shortcuts: 0,
          },
        })
      );
    });
  });
});

describe("<TopSiteLink>", () => {
  let globals;
  let link;
  let url;
  beforeEach(() => {
    globals = new GlobalOverrider();
    url = {
      createObjectURL: globals.sandbox.stub().returns(DEFAULT_BLOB_URL),
      revokeObjectURL: globals.sandbox.spy(),
    };
    globals.set("URL", url);
    link = { url: "https://foo.com", screenshot: "foo.jpg", hostname: "foo" };
  });
  afterEach(() => globals.restore());
  it("should add the right url", () => {
    link.url = "https://www.foobar.org";
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.propertyVal(
      wrapper.find("a").props(),
      "href",
      "https://www.foobar.org"
    );
  });
  it("should not add the url to the href if it a search shortcut", () => {
    link.searchTopSite = true;
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.isUndefined(wrapper.find("a").props().href);
  });
  it("should have rtl direction automatically set for text", () => {
    const wrapper = shallow(<TopSiteLink link={link} />);

    assert.isTrue(!!wrapper.find("[dir='auto']").length);
  });
  it("should render a title", () => {
    const wrapper = shallow(<TopSiteLink link={link} title="foobar" />);
    const titleEl = wrapper.find(".title");

    assert.equal(titleEl.text(), "foobar");
  });
  it("should have only the title as the text of the link", () => {
    const wrapper = shallow(<TopSiteLink link={link} title="foobar" />);

    assert.equal(wrapper.find("a").text(), "foobar");
  });
  it("should render the pin icon for pinned links", () => {
    link.isPinned = true;
    link.pinnedIndex = 7;
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.equal(wrapper.find(".icon-pin-small").length, 1);
  });
  it("should not render the pin icon for non pinned links", () => {
    link.isPinned = false;
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.equal(wrapper.find(".icon-pin-small").length, 0);
  });
  it("should render the first letter of the title as a fallback for missing icons", () => {
    const wrapper = shallow(<TopSiteLink link={link} title={"foo"} />);
    assert.equal(wrapper.find(".icon-wrapper").prop("data-fallback"), "f");
  });
  it("should render the tippy top icon if provided and not a small icon", () => {
    link.tippyTopIcon = "foo.png";
    link.backgroundColor = "#FFFFFF";
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.lengthOf(wrapper.find(".screenshot"), 0);
    assert.lengthOf(wrapper.find(".default-icon"), 0);
    const tippyTop = wrapper.find(".rich-icon");
    assert.propertyVal(
      tippyTop.props().style,
      "backgroundImage",
      "url(foo.png)"
    );
    assert.propertyVal(tippyTop.props().style, "backgroundColor", "#FFFFFF");
  });
  it("should render a rich icon if provided and not a small icon", () => {
    link.favicon = "foo.png";
    link.faviconSize = 196;
    link.backgroundColor = "#FFFFFF";
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.lengthOf(wrapper.find(".screenshot"), 0);
    assert.lengthOf(wrapper.find(".default-icon"), 0);
    const richIcon = wrapper.find(".rich-icon");
    assert.propertyVal(
      richIcon.props().style,
      "backgroundImage",
      "url(foo.png)"
    );
    assert.propertyVal(richIcon.props().style, "backgroundColor", "#FFFFFF");
  });
  it("should not render a rich icon if it is smaller than 96x96", () => {
    link.favicon = "foo.png";
    link.faviconSize = 48;
    link.backgroundColor = "#FFFFFF";
    const wrapper = shallow(<TopSiteLink link={link} />);
    assert.lengthOf(wrapper.find(".default-icon"), 1);
    assert.equal(wrapper.find(".rich-icon").length, 0);
  });
  it("should apply just the default class name to the outer link if props.className is falsey", () => {
    const wrapper = shallow(<TopSiteLink className={false} />);
    assert.ok(wrapper.find("li").hasClass("top-site-outer"));
  });
  it("should add props.className to the outer link element", () => {
    const wrapper = shallow(<TopSiteLink className="foo bar" />);
    assert.ok(wrapper.find("li").hasClass("top-site-outer foo bar"));
  });
  describe("#_allowDrop", () => {
    let wrapper;
    let event;
    beforeEach(() => {
      event = {
        dataTransfer: {
          types: ["text/topsite-index"],
        },
      };
      wrapper = shallow(
        <TopSiteLink isDraggable={true} onDragEvent={() => {}} />
      );
    });
    it("should be droppable for basic case", () => {
      const result = wrapper.instance()._allowDrop(event);
      assert.isTrue(result);
    });
    it("should not be droppable for sponsored_position", () => {
      wrapper.setProps({ link: { sponsored_position: 1 } });
      const result = wrapper.instance()._allowDrop(event);
      assert.isFalse(result);
    });
    it("should not be droppable for link.type", () => {
      wrapper.setProps({ link: { type: "SPOC" } });
      const result = wrapper.instance()._allowDrop(event);
      assert.isFalse(result);
    });
  });
  describe("#onDragEvent", () => {
    let simulate;
    let wrapper;
    beforeEach(() => {
      wrapper = shallow(
        <TopSiteLink isDraggable={true} onDragEvent={() => {}} />
      );
      simulate = type => {
        const event = {
          dataTransfer: { setData() {}, types: { includes() {} } },
          preventDefault() {
            this.prevented = true;
          },
          target: { blur() {} },
          type,
        };
        wrapper.simulate(type, event);
        return event;
      };
    });
    it("should allow clicks without dragging", () => {
      simulate("mousedown");
      simulate("mouseup");

      const event = simulate("click");

      assert.notOk(event.prevented);
    });
    it("should prevent clicks after dragging", () => {
      simulate("mousedown");
      simulate("dragstart");
      simulate("dragenter");
      simulate("drop");
      simulate("dragend");
      simulate("mouseup");

      const event = simulate("click");

      assert.ok(event.prevented);
    });
    it("should allow clicks after dragging then clicking", () => {
      simulate("mousedown");
      simulate("dragstart");
      simulate("dragenter");
      simulate("drop");
      simulate("dragend");
      simulate("mouseup");
      simulate("click");

      simulate("mousedown");
      simulate("mouseup");

      const event = simulate("click");

      assert.notOk(event.prevented);
    });
    it("should prevent dragging with sponsored_position from dragstart", () => {
      const preventDefault = sinon.stub();
      const blur = sinon.stub();
      wrapper.setProps({ link: { sponsored_position: 1 } });
      wrapper.instance().onDragEvent({
        type: "dragstart",
        preventDefault,
        target: { blur },
      });
      assert.calledOnce(preventDefault);
      assert.calledOnce(blur);
      assert.isUndefined(wrapper.instance().dragged);
    });
    it("should prevent dragging with link.shim from dragstart", () => {
      const preventDefault = sinon.stub();
      const blur = sinon.stub();
      wrapper.setProps({ link: { type: "SPOC" } });
      wrapper.instance().onDragEvent({
        type: "dragstart",
        preventDefault,
        target: { blur },
      });
      assert.calledOnce(preventDefault);
      assert.calledOnce(blur);
      assert.isUndefined(wrapper.instance().dragged);
    });
  });

  describe("#generateColor", () => {
    let colors;
    beforeEach(() => {
      colors = "#0090ED,#FF4F5F,#2AC3A2";
    });

    it("should generate a random color but always pick the same color for the same string", async () => {
      let wrapper = shallow(
        <TopSiteLink colors={colors} title={"food"} link={link} />
      );

      assert.equal(wrapper.find(".icon-wrapper").prop("data-fallback"), "f");
      assert.equal(
        wrapper.find(".icon-wrapper").prop("style").backgroundColor,
        colors.split(",")[1]
      );
      assert.ok(true);
    });

    it("should generate a different random color", async () => {
      let wrapper = shallow(
        <TopSiteLink colors={colors} title={"fam"} link={link} />
      );

      assert.equal(
        wrapper.find(".icon-wrapper").prop("style").backgroundColor,
        colors.split(",")[2]
      );
      assert.ok(true);
    });

    it("should generate a third random color", async () => {
      let wrapper = shallow(<TopSiteLink colors={colors} title={"foo"} />);

      assert.equal(wrapper.find(".icon-wrapper").prop("data-fallback"), "f");
      assert.equal(
        wrapper.find(".icon-wrapper").prop("style").backgroundColor,
        colors.split(",")[0]
      );
      assert.ok(true);
    });
  });
});

describe("<TopSite>", () => {
  let link;
  beforeEach(() => {
    link = { url: "https://foo.com", screenshot: "foo.jpg", hostname: "foo" };
  });

  // Build IntersectionObserver class with the arg `entries` for the intersect callback.
  function buildIntersectionObserver(entries) {
    return class {
      constructor(callback) {
        this.callback = callback;
      }

      observe() {
        this.callback(entries);
      }

      unobserve() {}
    };
  }

  it("should render a TopSite", () => {
    const wrapper = shallow(<TopSite link={link} />);
    assert.ok(wrapper.exists());
  });

  it("should render a shortened title based off the url", () => {
    link.url = "https://www.foobar.org";
    link.hostname = "foobar";
    link.eTLD = "org";
    const wrapper = shallow(<TopSite link={link} />);

    assert.equal(wrapper.find(TopSiteLink).props().title, "foobar");
  });

  it("should parse args for fluent correctly", () => {
    const title = '"fluent"';
    link.hostname = title;

    const wrapper = mount(<TopSite link={link} />);
    const button = wrapper.find(
      "button[data-l10n-id='newtab-menu-content-tooltip']"
    );
    assert.equal(button.prop("data-l10n-args"), JSON.stringify({ title }));
  });

  it("should have .active class, on top-site-outer if context menu is open", () => {
    const wrapper = shallow(<TopSite link={link} index={1} activeIndex={1} />);
    wrapper.setState({ showContextMenu: true });

    assert.equal(wrapper.find(TopSiteLink).props().className.trim(), "active");
  });
  it("should not add .active class, on top-site-outer if context menu is closed", () => {
    const wrapper = shallow(<TopSite link={link} index={1} />);
    wrapper.setState({ showContextMenu: false, activeTile: 1 });
    assert.equal(wrapper.find(TopSiteLink).props().className, "");
  });
  it("should render a context menu button", () => {
    const wrapper = shallow(<TopSite link={link} />);
    assert.equal(wrapper.find(ContextMenuButton).length, 1);
  });
  it("should render a link menu", () => {
    const wrapper = shallow(<TopSite link={link} />);
    assert.equal(wrapper.find(LinkMenu).length, 1);
  });
  it("should pass onUpdate, site, options, and index to LinkMenu", () => {
    const wrapper = shallow(<TopSite link={link} />);
    const linkMenuProps = wrapper.find(LinkMenu).props();
    ["onUpdate", "site", "index", "options"].forEach(prop =>
      assert.property(linkMenuProps, prop)
    );
  });
  it("should pass through the correct menu options to LinkMenu", () => {
    const wrapper = shallow(<TopSite link={link} />);
    const linkMenuProps = wrapper.find(LinkMenu).props();
    assert.deepEqual(linkMenuProps.options, [
      "CheckPinTopSite",
      "EditTopSite",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
      "DeleteUrl",
    ]);
  });
  it("should record impressions for visible organic Top Sites", () => {
    const dispatch = sinon.stub();
    const wrapper = shallow(
      <TopSite
        link={link}
        index={3}
        dispatch={dispatch}
        IntersectionObserver={buildIntersectionObserver([
          {
            isIntersecting: true,
            intersectionRatio: INTERSECTION_RATIO,
          },
        ])}
        document={{
          visibilityState: "visible",
          addEventListener: sinon.stub(),
          removeEventListener: sinon.stub(),
        }}
      />
    );
    const linkWrapper = wrapper.find(TopSiteLink).dive();
    assert.ok(linkWrapper.exists());
    const impressionWrapper = linkWrapper.find(TopSiteImpressionWrapper).dive();
    assert.ok(impressionWrapper.exists());

    assert.calledOnce(dispatch);

    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.TOP_SITES_ORGANIC_IMPRESSION_STATS);

    assert.propertyVal(action.data, "type", "impression");
    assert.propertyVal(action.data, "source", "newtab");
    assert.propertyVal(action.data, "position", 3);
  });
  it("should record impressions for visible sponsored Top Sites", () => {
    const dispatch = sinon.stub();
    const wrapper = shallow(
      <TopSite
        link={Object.assign({}, link, {
          sponsored_position: 2,
          sponsored_tile_id: 12345,
          sponsored_impression_url: "http://impression.example.com/",
        })}
        index={3}
        dispatch={dispatch}
        IntersectionObserver={buildIntersectionObserver([
          {
            isIntersecting: true,
            intersectionRatio: INTERSECTION_RATIO,
          },
        ])}
        document={{
          visibilityState: "visible",
          addEventListener: sinon.stub(),
          removeEventListener: sinon.stub(),
        }}
      />
    );
    const linkWrapper = wrapper.find(TopSiteLink).dive();
    assert.ok(linkWrapper.exists());
    const impressionWrapper = linkWrapper.find(TopSiteImpressionWrapper).dive();
    assert.ok(impressionWrapper.exists());

    assert.calledOnce(dispatch);

    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.TOP_SITES_SPONSORED_IMPRESSION_STATS);

    assert.propertyVal(action.data, "type", "impression");
    assert.propertyVal(action.data, "tile_id", 12345);
    assert.propertyVal(action.data, "source", "newtab");
    assert.propertyVal(action.data, "position", 3);
    assert.propertyVal(
      action.data,
      "reporting_url",
      "http://impression.example.com/"
    );
    assert.propertyVal(action.data, "advertiser", "foo");
  });

  describe("#onLinkClick", () => {
    it("should call dispatch when the link is clicked", () => {
      const dispatch = sinon.stub();
      const wrapper = shallow(
        <TopSite link={link} index={3} dispatch={dispatch} />
      );

      wrapper.find(TopSiteLink).simulate("click", { preventDefault() {} });

      let [action] = dispatch.firstCall.args;
      assert.isUserEventAction(action);

      assert.propertyVal(action.data, "event", "CLICK");
      assert.propertyVal(action.data, "source", "TOP_SITES");
      assert.propertyVal(action.data, "action_position", 3);

      [action] = dispatch.secondCall.args;
      assert.propertyVal(action, "type", at.OPEN_LINK);

      // Organic Top Site click event.
      [action] = dispatch.thirdCall.args;
      assert.equal(action.type, at.TOP_SITES_ORGANIC_IMPRESSION_STATS);

      assert.propertyVal(action.data, "type", "click");
      assert.propertyVal(action.data, "source", "newtab");
      assert.propertyVal(action.data, "position", 3);
    });
    it("should dispatch a UserEventAction with the right data", () => {
      const dispatch = sinon.stub();
      const wrapper = shallow(
        <TopSite
          link={Object.assign({}, link, {
            iconType: "rich_icon",
            isPinned: true,
          })}
          index={3}
          dispatch={dispatch}
        />
      );

      wrapper.find(TopSiteLink).simulate("click", { preventDefault() {} });

      const [action] = dispatch.firstCall.args;
      assert.isUserEventAction(action);

      assert.propertyVal(action.data, "event", "CLICK");
      assert.propertyVal(action.data, "source", "TOP_SITES");
      assert.propertyVal(action.data, "action_position", 3);
      assert.propertyVal(action.data.value, "card_type", "pinned");
      assert.propertyVal(action.data.value, "icon_type", "rich_icon");
    });
    it("should dispatch a UserEventAction with the right data for search top site", () => {
      const dispatch = sinon.stub();
      const siteInfo = {
        iconType: "tippytop",
        isPinned: true,
        searchTopSite: true,
        hostname: "google",
        label: "@google",
      };
      const wrapper = shallow(
        <TopSite
          link={Object.assign({}, link, siteInfo)}
          index={3}
          dispatch={dispatch}
        />
      );

      wrapper.find(TopSiteLink).simulate("click", { preventDefault() {} });

      const [action] = dispatch.firstCall.args;
      assert.isUserEventAction(action);

      assert.propertyVal(action.data, "event", "CLICK");
      assert.propertyVal(action.data, "source", "TOP_SITES");
      assert.propertyVal(action.data, "action_position", 3);
      assert.propertyVal(action.data.value, "card_type", "search");
      assert.propertyVal(action.data.value, "icon_type", "tippytop");
      assert.propertyVal(action.data.value, "search_vendor", "google");
    });
    it("should dispatch a UserEventAction with the right data for SPOC top site", () => {
      const dispatch = sinon.stub();
      const siteInfo = {
        id: 1,
        iconType: "custom_screenshot",
        type: "SPOC",
        pos: 1,
        label: "test advertiser",
        shim: { click: "shim_click_id" },
      };
      const wrapper = shallow(
        <TopSite
          link={Object.assign({}, link, siteInfo)}
          index={0}
          dispatch={dispatch}
        />
      );

      wrapper.find(TopSiteLink).simulate("click", { preventDefault() {} });

      let [action] = dispatch.firstCall.args;
      assert.isUserEventAction(action);

      assert.propertyVal(action.data, "event", "CLICK");
      assert.propertyVal(action.data, "source", "TOP_SITES");
      assert.propertyVal(action.data, "action_position", 0);
      assert.propertyVal(action.data.value, "card_type", "spoc");
      assert.propertyVal(action.data.value, "icon_type", "custom_screenshot");

      // Pocket SPOC click event.
      [action] = dispatch.getCall(2).args;
      assert.equal(action.type, at.TELEMETRY_IMPRESSION_STATS);

      assert.propertyVal(action.data, "click", 0);
      assert.propertyVal(action.data, "source", "TOP_SITES");

      [action] = dispatch.getCall(3).args;
      assert.equal(action.type, at.DISCOVERY_STREAM_USER_EVENT);

      assert.propertyVal(action.data, "event", "CLICK");
      assert.propertyVal(action.data, "action_position", 1);
      assert.propertyVal(action.data, "source", "TOP_SITES");
      assert.propertyVal(action.data.value, "card_type", "spoc");
      assert.propertyVal(action.data.value, "tile_id", 1);
      assert.propertyVal(action.data.value, "shim", "shim_click_id");

      // Topsite SPOC click event.
      [action] = dispatch.getCall(4).args;
      assert.equal(action.type, at.TOP_SITES_SPONSORED_IMPRESSION_STATS);

      assert.propertyVal(action.data, "type", "click");
      assert.propertyVal(action.data, "tile_id", 1);
      assert.propertyVal(action.data, "source", "newtab");
      assert.propertyVal(action.data, "position", 1);
      assert.propertyVal(action.data, "advertiser", "test advertiser");
    });
    it("should dispatch OPEN_LINK with the right data", () => {
      const dispatch = sinon.stub();
      const wrapper = shallow(
        <TopSite
          link={Object.assign({}, link, { typedBonus: true })}
          index={3}
          dispatch={dispatch}
        />
      );

      wrapper.find(TopSiteLink).simulate("click", { preventDefault() {} });

      const [action] = dispatch.secondCall.args;
      assert.propertyVal(action, "type", at.OPEN_LINK);
      assert.propertyVal(action.data, "typedBonus", true);
    });
  });
});

describe("<TopSiteForm>", () => {
  let wrapper;
  let sandbox;

  function setup(props = {}) {
    sandbox = sinon.createSandbox();
    const customProps = Object.assign(
      {},
      { onClose: sandbox.spy(), dispatch: sandbox.spy() },
      props
    );
    wrapper = mount(<TopSiteForm {...customProps} />);
  }

  describe("validateForm", () => {
    beforeEach(() => setup({ site: { url: "http://foo" } }));

    it("should return true for a correct URL", () => {
      wrapper.setState({ url: "foo" });

      assert.isTrue(wrapper.instance().validateForm());
    });

    it("should return false for a incorrect URL", () => {
      wrapper.setState({ url: " " });

      assert.isNull(wrapper.instance().validateForm());
      assert.isTrue(wrapper.state().validationError);
    });

    it("should return true for a correct custom screenshot URL", () => {
      wrapper.setState({ customScreenshotUrl: "foo" });

      assert.isTrue(wrapper.instance().validateForm());
    });

    it("should return false for a incorrect custom screenshot URL", () => {
      wrapper.setState({ customScreenshotUrl: " " });

      assert.isNull(wrapper.instance().validateForm());
    });

    it("should return true for an empty custom screenshot URL", () => {
      wrapper.setState({ customScreenshotUrl: "" });

      assert.isTrue(wrapper.instance().validateForm());
    });

    it("should return false for file: protocol", () => {
      wrapper.setState({ customScreenshotUrl: "file:///C:/Users/foo" });

      assert.isFalse(wrapper.instance().validateForm());
    });
  });

  describe("#previewButton", () => {
    beforeEach(() =>
      setup({
        site: { customScreenshotURL: "http://foo.com" },
        previewResponse: null,
      })
    );

    it("should render the preview button on invalid urls", () => {
      assert.equal(0, wrapper.find(".preview").length);

      wrapper.setState({ customScreenshotUrl: " " });

      assert.equal(1, wrapper.find(".preview").length);
    });

    it("should render the preview button when input value updated", () => {
      assert.equal(0, wrapper.find(".preview").length);

      wrapper.setState({
        customScreenshotUrl: "http://baz.com",
        screenshotPreview: null,
      });

      assert.equal(1, wrapper.find(".preview").length);
    });
  });

  describe("preview request", () => {
    beforeEach(() => {
      setup({
        site: { customScreenshotURL: "http://foo.com", url: "http://foo.com" },
        previewResponse: null,
      });
    });

    it("shouldn't dispatch a request for invalid urls", () => {
      wrapper.setState({ customScreenshotUrl: " ", url: "foo" });

      wrapper.find(".preview").simulate("click");

      assert.notCalled(wrapper.props().dispatch);
    });

    it("should dispatch a PREVIEW_REQUEST", () => {
      wrapper.setState({ customScreenshotUrl: "screenshot" });
      wrapper.find(".preview").simulate("submit");

      assert.calledTwice(wrapper.props().dispatch);
      assert.calledWith(
        wrapper.props().dispatch,
        ac.AlsoToMain({
          type: at.PREVIEW_REQUEST,
          data: { url: "http://screenshot" },
        })
      );
      assert.calledWith(
        wrapper.props().dispatch,
        ac.UserEvent({
          event: "PREVIEW_REQUEST",
          source: "TOP_SITES",
        })
      );
    });
  });

  describe("#TopSiteLink", () => {
    beforeEach(() => {
      setup();
    });

    it("should display a TopSiteLink preview", () => {
      assert.equal(wrapper.find(TopSiteLink).length, 1);
    });

    it("should display an icon for tippyTop sites", () => {
      wrapper.setProps({ site: { tippyTopIcon: "bar" } });

      assert.equal(
        wrapper.find(".top-site-icon").getDOMNode().style["background-image"],
        'url("bar")'
      );
    });

    it("should not display a preview screenshot", () => {
      wrapper.setProps({ previewResponse: "foo", previewUrl: "foo" });

      assert.lengthOf(wrapper.find(".screenshot"), 0);
    });

    it("should not render any icon on error", () => {
      wrapper.setProps({ previewResponse: "" });

      assert.equal(wrapper.find(".top-site-icon").length, 0);
    });

    it("should render the search icon when searchTopSite is true", () => {
      wrapper.setProps({ site: { tippyTopIcon: "bar", searchTopSite: true } });

      assert.equal(
        wrapper.find(".rich-icon").getDOMNode().style["background-image"],
        'url("bar")'
      );
      assert.isTrue(wrapper.find(".search-topsite").exists());
    });
  });

  describe("#addMode", () => {
    beforeEach(() => setup());

    it("should render the component", () => {
      assert.ok(wrapper.find(TopSiteForm).exists());
    });
    it("should have the correct header", () => {
      assert.equal(
        wrapper.findWhere(
          n =>
            n.length &&
            n.prop("data-l10n-id") === "newtab-topsites-add-shortcut-header"
        ).length,
        1
      );
    });
    it("should have the correct button text", () => {
      assert.equal(
        wrapper.findWhere(
          n =>
            n.length && n.prop("data-l10n-id") === "newtab-topsites-save-button"
        ).length,
        0
      );
      assert.equal(
        wrapper.findWhere(
          n =>
            n.length && n.prop("data-l10n-id") === "newtab-topsites-add-button"
        ).length,
        1
      );
    });
    it("should not render a preview button", () => {
      assert.equal(0, wrapper.find(".custom-image-input-container").length);
    });
    it("should call onClose if Cancel button is clicked", () => {
      wrapper.find(".cancel").simulate("click");
      assert.calledOnce(wrapper.instance().props.onClose);
    });
    it("should set validationError if url is empty", () => {
      assert.equal(wrapper.state().validationError, false);
      wrapper.find(".done").simulate("submit");
      assert.equal(wrapper.state().validationError, true);
    });
    it("should set validationError if url is invalid", () => {
      wrapper.setState({ url: "not valid" });
      assert.equal(wrapper.state().validationError, false);
      wrapper.find(".done").simulate("submit");
      assert.equal(wrapper.state().validationError, true);
    });
    it("should call onClose and dispatch with right args if URL is valid", () => {
      wrapper.setState({ url: "valid.com", label: "a label" });
      wrapper.find(".done").simulate("submit");
      assert.calledOnce(wrapper.instance().props.onClose);
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          site: { label: "a label", url: "http://valid.com" },
          index: -1,
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          action_position: -1,
          source: "TOP_SITES",
          event: "TOP_SITES_EDIT",
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TELEMETRY_USER_EVENT,
      });
    });
    it("should not pass empty string label in dispatch data", () => {
      wrapper.setState({ url: "valid.com", label: "" });
      wrapper.find(".done").simulate("submit");
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: { site: { url: "http://valid.com" }, index: -1 },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
    });
    it("should open the custom screenshot input", () => {
      assert.isFalse(wrapper.state().showCustomScreenshotForm);

      wrapper.find(A11yLinkButton).simulate("click");

      assert.isTrue(wrapper.state().showCustomScreenshotForm);
    });
  });

  describe("edit existing Topsite", () => {
    beforeEach(() =>
      setup({
        site: {
          url: "https://foo.bar",
          label: "baz",
          customScreenshotURL: "http://foo",
        },
        index: 7,
      })
    );

    it("should render the component", () => {
      assert.ok(wrapper.find(TopSiteForm).exists());
    });
    it("should have the correct header", () => {
      assert.equal(
        wrapper.findWhere(
          n => n.prop("data-l10n-id") === "newtab-topsites-edit-shortcut-header"
        ).length,
        1
      );
    });
    it("should have the correct button text", () => {
      assert.equal(
        wrapper.findWhere(
          n => n.prop("data-l10n-id") === "newtab-topsites-add-button"
        ).length,
        0
      );
      assert.equal(
        wrapper.findWhere(
          n => n.prop("data-l10n-id") === "newtab-topsites-save-button"
        ).length,
        1
      );
    });
    it("should call onClose if Cancel button is clicked", () => {
      wrapper.find(".cancel").simulate("click");
      assert.calledOnce(wrapper.instance().props.onClose);
    });
    it("should show error and not call onClose or dispatch if URL is empty", () => {
      wrapper.setState({ url: "" });
      assert.equal(wrapper.state().validationError, false);
      wrapper.find(".done").simulate("submit");
      assert.equal(wrapper.state().validationError, true);
      assert.notCalled(wrapper.instance().props.onClose);
      assert.notCalled(wrapper.instance().props.dispatch);
    });
    it("should show error and not call onClose or dispatch if URL is invalid", () => {
      wrapper.setState({ url: "not valid" });
      assert.equal(wrapper.state().validationError, false);
      wrapper.find(".done").simulate("submit");
      assert.equal(wrapper.state().validationError, true);
      assert.notCalled(wrapper.instance().props.onClose);
      assert.notCalled(wrapper.instance().props.dispatch);
    });
    it("should call onClose and dispatch with right args if URL is valid", () => {
      wrapper.find(".done").simulate("submit");
      assert.calledOnce(wrapper.instance().props.onClose);
      assert.calledTwice(wrapper.instance().props.dispatch);
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          site: {
            label: "baz",
            url: "https://foo.bar",
            customScreenshotURL: "http://foo",
          },
          index: 7,
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          action_position: 7,
          source: "TOP_SITES",
          event: "TOP_SITES_EDIT",
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TELEMETRY_USER_EVENT,
      });
    });
    it("should set customScreenshotURL to null if it was removed", () => {
      wrapper.setState({ customScreenshotUrl: "" });

      wrapper.find(".done").simulate("submit");

      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          site: {
            label: "baz",
            url: "https://foo.bar",
            customScreenshotURL: null,
          },
          index: 7,
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
    });
    it("should call onClose and dispatch with right args if URL is valid (negative index)", () => {
      wrapper.setProps({ index: -1 });
      wrapper.find(".done").simulate("submit");
      assert.calledOnce(wrapper.instance().props.onClose);
      assert.calledTwice(wrapper.instance().props.dispatch);
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          site: {
            label: "baz",
            url: "https://foo.bar",
            customScreenshotURL: "http://foo",
          },
          index: -1,
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
    });
    it("should not pass empty string label in dispatch data", () => {
      wrapper.setState({ label: "" });
      wrapper.find(".done").simulate("submit");
      assert.calledWith(wrapper.instance().props.dispatch, {
        data: {
          site: { url: "https://foo.bar", customScreenshotURL: "http://foo" },
          index: 7,
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: at.TOP_SITES_PIN,
      });
    });
    it("should render the save button if custom screenshot request finished", () => {
      wrapper.setState({
        customScreenshotUrl: "foo",
        screenshotPreview: "custom",
      });
      assert.equal(0, wrapper.find(".preview").length);
      assert.equal(1, wrapper.find(".done").length);
    });
    it("should render the save button if custom screenshot url was cleared", () => {
      wrapper.setState({ customScreenshotUrl: "" });
      wrapper.setProps({ site: { customScreenshotURL: "foo" } });
      assert.equal(0, wrapper.find(".preview").length);
      assert.equal(1, wrapper.find(".done").length);
    });
  });

  describe("#previewMode", () => {
    beforeEach(() => setup({ previewResponse: null }));

    it("should transition from save to preview", () => {
      wrapper.setProps({
        site: { url: "https://foo.bar", customScreenshotURL: "baz" },
        index: 7,
      });

      assert.equal(
        wrapper.findWhere(
          n =>
            n.length && n.prop("data-l10n-id") === "newtab-topsites-save-button"
        ).length,
        1
      );

      wrapper.setState({ customScreenshotUrl: "foo" });

      assert.equal(
        wrapper.findWhere(
          n =>
            n.length &&
            n.prop("data-l10n-id") === "newtab-topsites-preview-button"
        ).length,
        1
      );
    });

    it("should transition from add to preview", () => {
      assert.equal(
        wrapper.findWhere(
          n =>
            n.length && n.prop("data-l10n-id") === "newtab-topsites-add-button"
        ).length,
        1
      );

      wrapper.setState({ customScreenshotUrl: "foo" });

      assert.equal(
        wrapper.findWhere(
          n =>
            n.length &&
            n.prop("data-l10n-id") === "newtab-topsites-preview-button"
        ).length,
        1
      );
    });
  });

  describe("#validateUrl", () => {
    it("should properly validate URLs", () => {
      setup();
      assert.ok(wrapper.instance().validateUrl("mozilla.org"));
      assert.ok(wrapper.instance().validateUrl("https://mozilla.org"));
      assert.ok(wrapper.instance().validateUrl("http://mozilla.org"));
      assert.ok(
        wrapper
          .instance()
          .validateUrl(
            "https://mozilla.invisionapp.com/d/main/#/projects/prototypes"
          )
      );
      assert.ok(wrapper.instance().validateUrl("httpfoobar"));
      assert.ok(wrapper.instance().validateUrl("httpsfoo.bar"));
      assert.isNull(wrapper.instance().validateUrl("mozilla org"));
      assert.isNull(wrapper.instance().validateUrl(""));
    });
  });

  describe("#cleanUrl", () => {
    it("should properly prepend http:// to URLs when required", () => {
      setup();
      assert.equal(
        "http://mozilla.org",
        wrapper.instance().cleanUrl("mozilla.org")
      );
      assert.equal(
        "http://https.org",
        wrapper.instance().cleanUrl("https.org")
      );
      assert.equal("http://httpcom", wrapper.instance().cleanUrl("httpcom"));
      assert.equal(
        "http://mozilla.org",
        wrapper.instance().cleanUrl("http://mozilla.org")
      );
      assert.equal(
        "https://firefox.com",
        wrapper.instance().cleanUrl("https://firefox.com")
      );
    });
  });
});

describe("<TopSiteList>", () => {
  const APP = { isForStartupCache: false };

  it("should render a TopSiteList element", () => {
    const wrapper = shallow(<TopSiteList {...DEFAULT_PROPS} App={{ APP }} />);
    assert.ok(wrapper.exists());
  });
  it("should render a TopSite for each link with the right url", () => {
    const rows = [{ url: "https://foo.com" }, { url: "https://bar.com" }];
    const wrapper = shallow(
      <TopSiteList {...DEFAULT_PROPS} TopSites={{ rows }} App={{ APP }} />
    );
    const links = wrapper.find(TopSite);
    assert.lengthOf(links, 2);
    rows.forEach((row, i) =>
      assert.equal(links.get(i).props.link.url, row.url)
    );
  });
  it("should slice the TopSite rows to the TopSitesRows pref", () => {
    const rows = [];
    for (
      let i = 0;
      i < TOP_SITES_DEFAULT_ROWS * TOP_SITES_MAX_SITES_PER_ROW + 3;
      i++
    ) {
      rows.push({ url: `https://foo${i}.com` });
    }
    const wrapper = shallow(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={TOP_SITES_DEFAULT_ROWS}
        App={{ APP }}
      />
    );
    const links = wrapper.find(TopSite);
    assert.lengthOf(
      links,
      TOP_SITES_DEFAULT_ROWS * TOP_SITES_MAX_SITES_PER_ROW
    );
  });
  it("should add a single placeholder is there is availible space in the row", () => {
    const rows = [{ url: "https://foo.com" }, { url: "https://bar.com" }];
    const availibleRows = 1;
    const wrapper = shallow(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={availibleRows}
        App={{ APP }}
      />
    );
    assert.lengthOf(wrapper.find(TopSite), 2, "topSites");
    assert.lengthOf(
      wrapper.find(TopSitePlaceholder),
      availibleRows >= wrapper.find(TopSite).length ? 0 : 1,
      "placeholders"
    );
  });
  it("should fill sponsored top sites with placeholders while rendering for startup cache", () => {
    const rows = [
      { url: "https://sponsored01.com", sponsored_position: 1 },
      { url: "https://sponsored02.com", sponsored_position: 2 },
      { url: "https://sponsored03.com", type: "SPOC" },
      { url: "https://foo.com" },
      { url: "https://bar.com" },
    ];
    const wrapper = shallow(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={1}
        App={{ isForStartupCache: true }}
      />
    );
    assert.lengthOf(wrapper.find(TopSite), 2, "topSites");
    assert.lengthOf(wrapper.find(TopSitePlaceholder), 4, "placeholders");
  });
  it("should fill any holes in TopSites with placeholders", () => {
    const rows = [{ url: "https://foo.com" }];
    rows[3] = { url: "https://bar.com" };
    const wrapper = shallow(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={1}
        App={{ APP }}
      />
    );
    assert.lengthOf(wrapper.find(TopSite), 2, "topSites");
    assert.lengthOf(wrapper.find(TopSitePlaceholder), 1, "placeholders");
  });
  it("should update state onDragStart and clear it onDragEnd", () => {
    const wrapper = shallow(<TopSiteList {...DEFAULT_PROPS} App={{ APP }} />);
    const instance = wrapper.instance();
    const index = 7;
    const link = { url: "https://foo.com" };
    const title = "foo";
    instance.onDragEvent({ type: "dragstart" }, index, link, title);
    assert.equal(instance.state.draggedIndex, index);
    assert.equal(instance.state.draggedSite, link);
    assert.equal(instance.state.draggedTitle, title);
    instance.onDragEvent({ type: "dragend" });
    assert.deepEqual(instance.state, TopSiteList.DEFAULT_STATE);
  });
  it("should clear state when new props arrive after a drop", () => {
    const site1 = { url: "https://foo.com" };
    const site2 = { url: "https://bar.com" };
    const rows = [site1, site2];
    const wrapper = shallow(
      <TopSiteList {...DEFAULT_PROPS} TopSites={{ rows }} App={{ APP }} />
    );
    const instance = wrapper.instance();
    instance.setState({
      draggedIndex: 1,
      draggedSite: site2,
      draggedTitle: "bar",
      topSitesPreview: [],
    });
    wrapper.setProps({ TopSites: { rows: [site2, site1] } });
    assert.deepEqual(instance.state, TopSiteList.DEFAULT_STATE);
  });
  it("should dispatch events on drop", () => {
    const dispatch = sinon.spy();
    const wrapper = shallow(
      <TopSiteList {...DEFAULT_PROPS} dispatch={dispatch} App={{ APP }} />
    );
    const instance = wrapper.instance();
    const index = 7;
    const link = { url: "https://foo.com", customScreenshotURL: "foo" };
    const title = "foo";
    instance.onDragEvent({ type: "dragstart" }, index, link, title);
    dispatch.resetHistory();
    instance.onDragEvent({ type: "drop" }, 3);
    assert.calledTwice(dispatch);
    assert.calledWith(dispatch, {
      data: {
        draggedFromIndex: 7,
        index: 3,
        site: {
          label: "foo",
          url: "https://foo.com",
          customScreenshotURL: "foo",
        },
      },
      meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
      type: "TOP_SITES_INSERT",
    });
    assert.calledWith(dispatch, {
      data: { action_position: 3, event: "DROP", source: "TOP_SITES" },
      meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
      type: "TELEMETRY_USER_EVENT",
    });
  });
  it("should make a topSitesPreview onDragEnter", () => {
    const wrapper = shallow(<TopSiteList {...DEFAULT_PROPS} App={{ APP }} />);
    const instance = wrapper.instance();
    const site = { url: "https://foo.com" };
    instance.setState({
      draggedIndex: 4,
      draggedSite: site,
      draggedTitle: "foo",
    });
    const draggedSite = Object.assign({}, site, {
      isPinned: true,
      isDragged: true,
    });
    instance.onDragEvent({ type: "dragenter" }, 2);
    assert.ok(instance.state.topSitesPreview);
    assert.deepEqual(instance.state.topSitesPreview[2], draggedSite);
  });
  it("should _makeTopSitesPreview correctly", () => {
    const site1 = { url: "https://foo.com" };
    const site2 = { url: "https://bar.com" };
    const site3 = { url: "https://baz.com" };
    const rows = [site1, site2, site3];
    let wrapper = shallow(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={1}
        App={{ APP }}
      />
    );
    const addButton = { isAddButton: true };
    let instance = wrapper.instance();
    instance.setState({
      draggedIndex: 0,
      draggedSite: site1,
      draggedTitle: "foo",
    });
    let draggedSite = Object.assign({}, site1, {
      isPinned: true,
      isDragged: true,
    });
    assert.deepEqual(instance._makeTopSitesPreview(1), [
      site2,
      draggedSite,
      site3,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(2), [
      site2,
      site3,
      draggedSite,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(3), [
      site2,
      site3,
      addButton,
      draggedSite,
      null,
      null,
      null,
      null,
    ]);
    site2.isPinned = true;
    assert.deepEqual(instance._makeTopSitesPreview(1), [
      site2,
      draggedSite,
      site3,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(2), [
      site3,
      site2,
      draggedSite,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    site3.isPinned = true;
    assert.deepEqual(instance._makeTopSitesPreview(1), [
      site2,
      draggedSite,
      site3,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(2), [
      site2,
      site3,
      draggedSite,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    site2.isPinned = false;
    assert.deepEqual(instance._makeTopSitesPreview(1), [
      site2,
      draggedSite,
      site3,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(2), [
      site2,
      site3,
      draggedSite,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    site3.isPinned = false;
    instance.setState({
      draggedIndex: 1,
      draggedSite: site2,
      draggedTitle: "bar",
    });
    draggedSite = Object.assign({}, site2, { isPinned: true, isDragged: true });
    assert.deepEqual(instance._makeTopSitesPreview(0), [
      draggedSite,
      site1,
      site3,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    assert.deepEqual(instance._makeTopSitesPreview(2), [
      site1,
      site3,
      draggedSite,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    site2.type = "SPOC";
    instance.setState({
      draggedIndex: 2,
      draggedSite: site3,
      draggedTitle: "baz",
    });
    draggedSite = Object.assign({}, site3, { isPinned: true, isDragged: true });
    assert.deepEqual(instance._makeTopSitesPreview(0), [
      draggedSite,
      site2,
      site1,
      addButton,
      null,
      null,
      null,
      null,
    ]);
    site2.type = "";
    site2.sponsored_position = 2;
    instance.setState({
      draggedIndex: 2,
      draggedSite: site3,
      draggedTitle: "baz",
    });
    draggedSite = Object.assign({}, site3, { isPinned: true, isDragged: true });
    assert.deepEqual(instance._makeTopSitesPreview(0), [
      draggedSite,
      site2,
      site1,
      addButton,
      null,
      null,
      null,
      null,
    ]);
  });
  it("should add a className hide-for-narrow to sites after 6/row", () => {
    const rows = [];
    for (let i = 0; i < TOP_SITES_MAX_SITES_PER_ROW; i++) {
      rows.push({ url: `https://foo${i}.com` });
    }
    const wrapper = mount(
      <TopSiteList
        {...DEFAULT_PROPS}
        TopSites={{ rows }}
        TopSitesRows={1}
        App={{ APP }}
      />
    );
    assert.lengthOf(wrapper.find("li.hide-for-narrow"), 2);
  });
});

describe("TopSitePlaceholder", () => {
  it("should dispatch a TOP_SITES_EDIT action when the addbutton is clicked", () => {
    const dispatch = sinon.spy();
    const wrapper = shallow(
      <TopSitePlaceholder dispatch={dispatch} index={7} isAddButton={true} />
    );

    wrapper.find(".add-button").first().simulate("click");

    assert.calledOnce(dispatch);
    assert.calledWithExactly(dispatch, {
      type: at.TOP_SITES_EDIT,
      data: { index: 7 },
    });
  });
});

describe("#TopSiteFormInput", () => {
  let wrapper;
  let onChangeStub;

  describe("no errors", () => {
    beforeEach(() => {
      onChangeStub = sinon.stub();

      wrapper = mount(
        <TopSiteFormInput
          titleId="newtab-topsites-title-label"
          placeholderId="newtab-topsites-title-input"
          errorMessageId="newtab-topsites-url-validation"
          onChange={onChangeStub}
          value="foo"
        />
      );
    });

    it("should render the provided title", () => {
      const title = wrapper.find("span");
      assert.propertyVal(
        title.props(),
        "data-l10n-id",
        "newtab-topsites-title-label"
      );
    });

    it("should render the provided value", () => {
      const input = wrapper.find("input");

      assert.equal(input.getDOMNode().value, "foo");
    });

    it("should render the clear button if cb is provided", () => {
      assert.equal(wrapper.find(".icon-clear-input").length, 0);

      wrapper.setProps({ onClear: sinon.stub() });

      assert.equal(wrapper.find(".icon-clear-input").length, 1);
    });

    it("should show the loading indicator", () => {
      assert.equal(wrapper.find(".loading-container").length, 0);

      wrapper.setProps({ loading: true });

      assert.equal(wrapper.find(".loading-container").length, 1);
    });
    it("should disable the input when loading indicator is present", () => {
      assert.isFalse(wrapper.find("input").getDOMNode().disabled);

      wrapper.setProps({ loading: true });

      assert.isTrue(wrapper.find("input").getDOMNode().disabled);
    });
  });

  describe("with error", () => {
    beforeEach(() => {
      onChangeStub = sinon.stub();

      wrapper = mount(
        <TopSiteFormInput
          titleId="newtab-topsites-title-label"
          placeholderId="newtab-topsites-title-input"
          onChange={onChangeStub}
          validationError={true}
          errorMessageId="newtab-topsites-url-validation"
          value="foo"
        />
      );
    });

    it("should render the error message", () => {
      assert.equal(
        wrapper.findWhere(
          n => n.prop("data-l10n-id") === "newtab-topsites-url-validation"
        ).length,
        1
      );
    });

    it("should reset the error state on value change", () => {
      wrapper.find("input").simulate("change", { target: { value: "bar" } });

      assert.isFalse(wrapper.state().validationError);
    });
  });
});
