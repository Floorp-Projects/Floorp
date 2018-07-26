import {combineReducers, createStore} from "redux";
import {INITIAL_STATE, reducers} from "common/Reducers.jsm";
import {mountWithIntl, shallowWithIntl} from "test/unit/utils";
import {Section, SectionIntl, _Sections as Sections} from "content-src/components/Sections/Sections";
import {actionTypes as at} from "common/Actions.jsm";
import {PlaceholderCard} from "content-src/components/Card/Card";
import {Provider} from "react-redux";
import React from "react";
import {SectionMenu} from "content-src/components/SectionMenu/SectionMenu";
import {shallow} from "enzyme";
import {TopSites} from "content-src/components/TopSites/TopSites";

function mountSectionWithProps(props) {
  const store = createStore(combineReducers(reducers), INITIAL_STATE);
  return mountWithIntl(<Provider store={store}><Section {...props} /></Provider>);
}

describe("<Sections>", () => {
  let wrapper;
  let FAKE_SECTIONS;
  beforeEach(() => {
    FAKE_SECTIONS = new Array(5).fill(null).map((v, i) => ({
      id: `foo_bar_${i}`,
      title: `Foo Bar ${i}`,
      enabled: !!(i % 2),
      rows: []
    }));
    wrapper = shallow(<Sections Sections={FAKE_SECTIONS} Prefs={{values: {sectionOrder: FAKE_SECTIONS.map(i => i.id).join(",")}}} />);
  });
  it("should render a Sections element", () => {
    assert.ok(wrapper.exists());
  });
  it("should render a Section for each one passed in props.Sections with .enabled === true", () => {
    const sectionElems = wrapper.find(SectionIntl);
    assert.lengthOf(sectionElems, 2);
    sectionElems.forEach((section, i) => {
      assert.equal(section.props().id, FAKE_SECTIONS[2 * i + 1].id);
      assert.equal(section.props().enabled, true);
    });
  });
  it("should render Top Sites if feeds.topsites pref is true", () => {
    wrapper = shallow(<Sections Sections={FAKE_SECTIONS} Prefs={{values: {"feeds.topsites": true, "sectionOrder": "topsites,topstories,highlights"}}} />);
    assert.equal(wrapper.find(TopSites).length, 1);
  });
  it("should NOT render Top Sites if feeds.topsites pref is false", () => {
    wrapper = shallow(<Sections Sections={FAKE_SECTIONS} Prefs={{values: {"feeds.topsites": false, "sectionOrder": "topsites,topstories,highlights"}}} />);
    assert.equal(wrapper.find(TopSites).length, 0);
  });
  it("should render the sections in the order specifed by sectionOrder pref", () => {
    wrapper = shallow(<Sections Sections={FAKE_SECTIONS} Prefs={{values: {sectionOrder: "foo_bar_1,foo_bar_3"}}} />);
    let sections = wrapper.find(SectionIntl);
    assert.lengthOf(sections, 2);
    assert.equal(sections.first().props().id, "foo_bar_1");
    assert.equal(sections.last().props().id, "foo_bar_3");
    wrapper = shallow(<Sections Sections={FAKE_SECTIONS} Prefs={{values: {sectionOrder: "foo_bar_3,foo_bar_1"}}} />);
    sections = wrapper.find(SectionIntl);
    assert.lengthOf(sections, 2);
    assert.equal(sections.first().props().id, "foo_bar_3");
    assert.equal(sections.last().props().id, "foo_bar_1");
  });
});

describe("<Section>", () => {
  let wrapper;
  let FAKE_SECTION;

  beforeEach(() => {
    FAKE_SECTION = {
      id: `foo_bar_1`,
      pref: {collapsed: false},
      title: `Foo Bar 1`,
      rows: [{link: "http://localhost", index: 0}],
      emptyState: {
        icon: "check",
        message: "Some message"
      },
      rowsPref: "section.rows",
      maxRows: 4,
      Prefs: {values: {"section.rows": 2}}
    };
    wrapper = mountSectionWithProps(FAKE_SECTION);
  });

  describe("context menu", () => {
    it("should render a context menu button", () => {
      wrapper = mountSectionWithProps(FAKE_SECTION);

      assert.equal(wrapper.find(".section-top-bar .context-menu-button").length, 1);
    });
    it("should render a section menu when button is clicked", () => {
      wrapper = mountSectionWithProps(FAKE_SECTION);

      const button = wrapper.find(".section-top-bar .context-menu-button");
      assert.equal(wrapper.find(SectionMenu).length, 0);
      button.simulate("click", {preventDefault: () => {}});
      assert.equal(wrapper.find(SectionMenu).length, 1);
    });
    it("should not render a section menu by default", () => {
      wrapper = shallowWithIntl(<Section {...FAKE_SECTION} />);
      assert.equal(wrapper.find(SectionMenu).length, 0);
    });
  });

  describe("placeholders", () => {
    const CARDS_PER_ROW = 3;
    const fakeSite = {link: "http://localhost"};
    function renderWithSites(rows) {
      const store = createStore(combineReducers(reducers), INITIAL_STATE);
      return mountWithIntl(<Provider store={store}><Section {...FAKE_SECTION} rows={rows} /></Provider>);
    }

    it("should return 2 row of placeholders if realRows is 0", () => {
      wrapper = renderWithSites([]);
      assert.lengthOf(wrapper.find(PlaceholderCard), 6);
    });
    it("should fill in the rest of the rows", () => {
      wrapper = renderWithSites(new Array(CARDS_PER_ROW).fill(fakeSite));
      assert.lengthOf(wrapper.find(PlaceholderCard), CARDS_PER_ROW, "CARDS_PER_ROW");

      wrapper = renderWithSites(new Array(CARDS_PER_ROW + 1).fill(fakeSite));
      assert.lengthOf(wrapper.find(PlaceholderCard), 2, "CARDS_PER_ROW + 1");

      wrapper = renderWithSites(new Array(CARDS_PER_ROW + 2).fill(fakeSite));
      assert.lengthOf(wrapper.find(PlaceholderCard), 1, "CARDS_PER_ROW + 2");

      wrapper = renderWithSites(new Array(2 * CARDS_PER_ROW - 1).fill(fakeSite));
      assert.lengthOf(wrapper.find(PlaceholderCard), 1, "CARDS_PER_ROW - 1");
    });
    it("should not add placeholders all the rows are full", () => {
      wrapper = renderWithSites(new Array(2 * CARDS_PER_ROW).fill(fakeSite));
      assert.lengthOf(wrapper.find(PlaceholderCard), 0, "2 rows");
    });
  });

  describe("empty state", () => {
    beforeEach(() => {
      Object.assign(FAKE_SECTION, {
        initialized: true,
        rows: [],
        emptyState: {message: "Some message", icon: "moz-extension://some/extension/path"}
      });
      wrapper = shallowWithIntl(
        <Section {...FAKE_SECTION} />);
    });
    it("should be shown when rows is empty and initialized is true", () => {
      assert.ok(wrapper.find(".empty-state").exists());
    });
    it("should not be shown in initialized is false", () => {
      Object.assign(FAKE_SECTION, {
        initialized: false,
        rows: [],
        emptyState: {message: "Some message", icon: "moz-extension://some/extension/path"}
      });
      wrapper = shallowWithIntl(
        <Section {...FAKE_SECTION} />);
      assert.isFalse(wrapper.find(".empty-state").exists());
    });
    it("should use the icon prop as the icon url if it starts with `moz-extension://`", () => {
      const props = wrapper.find(".empty-state-icon").first().props();
      assert.equal(props.style["background-image"], `url('${FAKE_SECTION.emptyState.icon}')`);
    });
  });

  describe("topics component", () => {
    let TOP_STORIES_SECTION;
    beforeEach(() => {
      TOP_STORIES_SECTION = {
        id: "topstories",
        title: "TopStories",
        pref: {collapsed: false},
        rows: [{guid: 1, link: "http://localhost", isDefault: true}],
        topics: [],
        read_more_endpoint: "http://localhost/read-more",
        maxRows: 1,
        eventSource: "TOP_STORIES"
      };
    });
    it("should not render for empty topics", () => {
      wrapper = mountSectionWithProps(TOP_STORIES_SECTION);

      assert.lengthOf(wrapper.find(".topic"), 0);
    });
    it("should render for non-empty topics", () => {
      TOP_STORIES_SECTION.topics = [{name: "topic1", url: "topic-url1"}];

      wrapper = mountSectionWithProps(TOP_STORIES_SECTION);

      assert.lengthOf(wrapper.find(".topic"), 1);
    });
    it("should render for uninitialized topics", () => {
      delete TOP_STORIES_SECTION.topics;

      wrapper = mountSectionWithProps(TOP_STORIES_SECTION);

      assert.lengthOf(wrapper.find(".topic"), 1);
    });
  });

  describe("impression stats", () => {
    const FAKE_TOPSTORIES_SECTION_PROPS = {
      id: "TopStories",
      title: "Foo Bar 1",
      pref: {collapsed: false},
      maxRows: 1,
      rows: [{guid: 1}, {guid: 2}],
      shouldSendImpressionStats: true,

      document: {
        visibilityState: "visible",
        addEventListener: sinon.stub(),
        removeEventListener: sinon.stub()
      },
      eventSource: "TOP_STORIES",
      options: {personalized: false}
    };

    function renderSection(props = {}) {
      return shallowWithIntl(<Section
        {...FAKE_TOPSTORIES_SECTION_PROPS}
        {...props} />);
    }

    it("should send impression with the right stats when the page loads", () => {
      const dispatch = sinon.spy();
      renderSection({dispatch});

      assert.calledOnce(dispatch);

      const [action] = dispatch.firstCall.args;
      assert.equal(action.type, at.TELEMETRY_IMPRESSION_STATS);
      assert.equal(action.data.source, "TOP_STORIES");
      assert.deepEqual(action.data.tiles, [{id: 1}, {id: 2}]);
    });
    it("should not send impression stats if not configured", () => {
      const dispatch = sinon.spy();
      const props = Object.assign({}, FAKE_TOPSTORIES_SECTION_PROPS, {shouldSendImpressionStats: false, dispatch});
      renderSection(props);
      assert.notCalled(dispatch);
    });
    it("should not send impression stats if the section is collapsed", () => {
      const dispatch = sinon.spy();
      const props = Object.assign({}, FAKE_TOPSTORIES_SECTION_PROPS, {pref: {collapsed: true}});
      renderSection(props);
      assert.notCalled(dispatch);
    });
    it("should send 1 impression when the page becomes visibile after loading", () => {
      const props = {
        dispatch: sinon.spy(),
        document: {
          visibilityState: "hidden",
          addEventListener: sinon.spy(),
          removeEventListener: sinon.spy()
        }
      };

      renderSection(props);

      // Was the event listener added?
      assert.calledWith(props.document.addEventListener, "visibilitychange");

      // Make sure dispatch wasn't called yet
      assert.notCalled(props.dispatch);

      // Simulate a visibilityChange event
      const [, listener] = props.document.addEventListener.firstCall.args;
      props.document.visibilityState = "visible";
      listener();

      // Did we actually dispatch an event?
      assert.calledOnce(props.dispatch);
      assert.equal(props.dispatch.firstCall.args[0].type, at.TELEMETRY_IMPRESSION_STATS);

      // Did we remove the event listener?
      assert.calledWith(props.document.removeEventListener, "visibilitychange", listener);
    });
    it("should remove visibility change listener when section is removed", () => {
      const props = {
        dispatch: sinon.spy(),
        document: {
          visibilityState: "hidden",
          addEventListener: sinon.spy(),
          removeEventListener: sinon.spy()
        }
      };

      const section = renderSection(props);
      assert.calledWith(props.document.addEventListener, "visibilitychange");
      const [, listener] = props.document.addEventListener.firstCall.args;

      section.unmount();
      assert.calledWith(props.document.removeEventListener, "visibilitychange", listener);
    });
    it("should send an impression if props are updated and props.rows are different", () => {
      const props = {dispatch: sinon.spy()};
      wrapper = renderSection(props);
      props.dispatch.reset();

      // New rows
      wrapper.setProps(Object.assign({},
        FAKE_TOPSTORIES_SECTION_PROPS,
        {rows: [{guid: 123}]}
      ));

      assert.calledOnce(props.dispatch);
    });
    it("should not send an impression if props are updated but props.rows are the same", () => {
      const props = {dispatch: sinon.spy()};
      wrapper = renderSection(props);
      props.dispatch.reset();

      // Only update the disclaimer prop
      wrapper.setProps(Object.assign({},
        FAKE_TOPSTORIES_SECTION_PROPS,
        {disclaimer: {id: "bar"}}
      ));

      assert.notCalled(props.dispatch);
    });
    it("should not send an impression if props are updated and props.rows are the same but section is collapsed", () => {
      const props = {dispatch: sinon.spy()};
      wrapper = renderSection(props);
      props.dispatch.reset();

      // New rows and collapsed
      wrapper.setProps(Object.assign({},
        FAKE_TOPSTORIES_SECTION_PROPS,
        {
          rows: [{guid: 123}],
          pref: {collapsed: true}
        }
      ));

      assert.notCalled(props.dispatch);

      // Expand the section. Now the impression stats should be sent
      wrapper.setProps(Object.assign({},
        FAKE_TOPSTORIES_SECTION_PROPS,
        {
          rows: [{guid: 123}],
          pref: {collapsed: false}
        }
      ));

      assert.calledOnce(props.dispatch);
    });
    it("should not send an impression if props are updated but GUIDs are the same", () => {
      const props = {dispatch: sinon.spy()};
      wrapper = renderSection(props);
      props.dispatch.reset();

      wrapper.setProps(Object.assign({},
        FAKE_TOPSTORIES_SECTION_PROPS,
        {rows: [{guid: 1}, {guid: 2}]}
      ));

      assert.notCalled(props.dispatch);
    });
    it("should only send the latest impression on a visibility change", () => {
      const listeners = new Set();
      const props = {
        dispatch: sinon.spy(),
        document: {
          visibilityState: "hidden",
          addEventListener: (ev, cb) => listeners.add(cb),
          removeEventListener: (ev, cb) => listeners.delete(cb)
        }
      };

      wrapper = renderSection(props);

      // Update twice
      wrapper.setProps(Object.assign({}, props,
        {rows: [{guid: 123}]}
      ));
      wrapper.setProps(Object.assign({}, props,
        {rows: [{guid: 2432}]}
      ));

      assert.notCalled(props.dispatch);

      // Simulate listeners getting called
      props.document.visibilityState = "visible";
      listeners.forEach(l => l());

      // Make sure we only sent the latest event
      assert.calledOnce(props.dispatch);
      const [action] = props.dispatch.firstCall.args;
      assert.deepEqual(action.data.tiles, [{id: 2432}]);
    });
  });

  describe("#numRows", () => {
    it("should return maxRows if there is no rowsPref set", () => {
      delete FAKE_SECTION.rowsPref;
      wrapper = mountSectionWithProps(FAKE_SECTION);
      assert.equal(wrapper.find(Section).instance().numRows, FAKE_SECTION.maxRows);
    });

    it("should return number of rows set in Pref if rowsPref is set", () => {
      const numRows = 2;
      Object.assign(FAKE_SECTION, {
        rowsPref: "section.rows",
        maxRows: 4,
        Prefs: {values: {"section.rows": numRows}}
      });
      wrapper = mountSectionWithProps(FAKE_SECTION);
      assert.equal(wrapper.find(Section).instance().numRows, numRows);
    });

    it("should return number of rows set in Pref even if higher than maxRows value", () => {
      const numRows = 10;
      Object.assign(FAKE_SECTION, {
        rowsPref: "section.rows",
        maxRows: 4,
        Prefs: {values: {"section.rows": numRows}}
      });
      wrapper = mountSectionWithProps(FAKE_SECTION);
      assert.equal(wrapper.find(Section).instance().numRows, numRows);
    });
  });
});
