"use strict";
const {SectionsFeed, SectionsManager} = require("lib/SectionsManager.jsm");
const {EventEmitter, GlobalOverrider} = require("test/unit/utils");
const {MAIN_MESSAGE_TYPE, CONTENT_MESSAGE_TYPE} = require("common/Actions.jsm");

const FAKE_ID = "FAKE_ID";
const FAKE_OPTIONS = {icon: "FAKE_ICON", title: "FAKE_TITLE"};
const FAKE_ROWS = [{url: "1"}, {url: "2"}, {"url": "3"}];

let globals;

beforeEach(() => {
  globals = new GlobalOverrider();
});

afterEach(() => {
  globals.restore();
  // Redecorate SectionsManager to remove any listeners that have been added
  EventEmitter.decorate(SectionsManager);
  SectionsManager.init();
});

describe("SectionsManager", () => {
  describe("#init", () => {
    it("should initialise the sections map with the built in sections", () => {
      SectionsManager.sections.clear();
      SectionsManager.initialized = false;
      SectionsManager.init();
      assert.equal(SectionsManager.sections.size, 1);
      assert.ok(SectionsManager.sections.has("topstories"));
    });
    it("should set .initialized to true", () => {
      SectionsManager.sections.clear();
      SectionsManager.initialized = false;
      SectionsManager.init();
      assert.ok(SectionsManager.initialized);
    });
  });
  describe("#addBuiltInSection", () => {
    it("should not report an error if options is undefined", () => {
      globals.sandbox.spy(global.Components.utils, "reportError");
      SectionsManager.addBuiltInSection("feeds.section.topstories", undefined);
      assert.notCalled(Components.utils.reportError);
    });
    it("should report an error if options is malformed", () => {
      globals.sandbox.spy(global.Components.utils, "reportError");
      SectionsManager.addBuiltInSection("feeds.section.topstories", "invalid");
      assert.calledOnce(Components.utils.reportError);
    });
  });
  describe("#addSection", () => {
    it("should add the id to sections and emit an ADD_SECTION event", () => {
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.ADD_SECTION, spy);
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      assert.ok(SectionsManager.sections.has(FAKE_ID));
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.ADD_SECTION, FAKE_ID, FAKE_OPTIONS);
    });
  });
  describe("#removeSection", () => {
    it("should remove the id from sections and emit an REMOVE_SECTION event", () => {
      // Ensure we start with the id in the set
      assert.ok(SectionsManager.sections.has(FAKE_ID));
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.REMOVE_SECTION, spy);
      SectionsManager.removeSection(FAKE_ID);
      assert.notOk(SectionsManager.sections.has(FAKE_ID));
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.REMOVE_SECTION, FAKE_ID);
    });
  });
  describe("#enableSection", () => {
    it("should call updateSection with {enabled: true}", () => {
      sinon.spy(SectionsManager, "updateSection");
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      SectionsManager.enableSection(FAKE_ID);
      assert.calledOnce(SectionsManager.updateSection);
      assert.calledWith(SectionsManager.updateSection, FAKE_ID, {enabled: true}, true);
      SectionsManager.updateSection.restore();
    });
  });
  describe("#disableSection", () => {
    it("should call updateSection with {enabled: false, rows: []}", () => {
      sinon.spy(SectionsManager, "updateSection");
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      SectionsManager.disableSection(FAKE_ID);
      assert.calledOnce(SectionsManager.updateSection);
      assert.calledWith(SectionsManager.updateSection, FAKE_ID, {enabled: false, rows: []}, true);
      SectionsManager.updateSection.restore();
    });
  });
  describe("#updateSection", () => {
    it("should emit an UPDATE_SECTION event with correct arguments", () => {
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.updateSection(FAKE_ID, {rows: FAKE_ROWS}, true);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.UPDATE_SECTION, FAKE_ID, {rows: FAKE_ROWS}, true);
    });
    it("should do nothing if the section doesn't exist", () => {
      SectionsManager.removeSection(FAKE_ID);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.updateSection(FAKE_ID, {rows: FAKE_ROWS}, true);
      assert.notCalled(spy);
    });
  });
  describe("#onceInitialized", () => {
    it("should call the callback immediately if SectionsManager is initialised", () => {
      SectionsManager.initialized = true;
      const callback = sinon.spy();
      SectionsManager.onceInitialized(callback);
      assert.calledOnce(callback);
    });
    it("should bind the callback to .once(INIT) if SectionsManager is not initialised", () => {
      SectionsManager.initialized = false;
      sinon.spy(SectionsManager, "once");
      const callback = () => {};
      SectionsManager.onceInitialized(callback);
      assert.calledOnce(SectionsManager.once);
      assert.calledWith(SectionsManager.once, SectionsManager.INIT, callback);
    });
  });
});

describe("SectionsFeed", () => {
  let feed;

  beforeEach(() => {
    SectionsManager.sections.clear();
    SectionsManager.initialized = false;
    feed = new SectionsFeed();
    feed.store = {dispatch: sinon.spy()};
  });
  afterEach(() => {
    feed.uninit();
  });
  describe("#init", () => {
    it("should create a SectionsFeed", () => {
      assert.instanceOf(feed, SectionsFeed);
    });
    it("should bind appropriate listeners", () => {
      sinon.spy(SectionsManager, "on");
      feed.init();
      assert.calledThrice(SectionsManager.on);
      for (const [event, listener] of [
        [SectionsManager.ADD_SECTION, feed.onAddSection],
        [SectionsManager.REMOVE_SECTION, feed.onRemoveSection],
        [SectionsManager.UPDATE_SECTION, feed.onUpdateSection]
      ]) {
        assert.calledWith(SectionsManager.on, event, listener);
      }
    });
    it("should call onAddSection for any already added sections in SectionsManager", () => {
      SectionsManager.init();
      assert.ok(SectionsManager.sections.has("topstories"));
      const topstories = SectionsManager.sections.get("topstories");
      sinon.spy(feed, "onAddSection");
      feed.init();
      assert.calledOnce(feed.onAddSection);
      assert.calledWith(feed.onAddSection, SectionsManager.ADD_SECTION, "topstories", topstories);
    });
  });
  describe("#uninit", () => {
    it("should unbind all listeners", () => {
      sinon.spy(SectionsManager, "off");
      feed.init();
      feed.uninit();
      assert.calledThrice(SectionsManager.off);
      for (const [event, listener] of [
        [SectionsManager.ADD_SECTION, feed.onAddSection],
        [SectionsManager.REMOVE_SECTION, feed.onRemoveSection],
        [SectionsManager.UPDATE_SECTION, feed.onUpdateSection]
      ]) {
        assert.calledWith(SectionsManager.off, event, listener);
      }
    });
    it("should emit an UNINIT event and set SectionsManager.initialized to false", () => {
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UNINIT, spy);
      feed.init();
      feed.uninit();
      assert.calledOnce(spy);
      assert.notOk(SectionsManager.initialized);
    });
  });
  describe("#onAddSection", () => {
    it("should broadcast a SECTION_REGISTER action with the correct data", () => {
      feed.onAddSection(null, FAKE_ID, FAKE_OPTIONS);
      const action = feed.store.dispatch.firstCall.args[0];
      assert.equal(action.type, "SECTION_REGISTER");
      assert.deepEqual(action.data, Object.assign({id: FAKE_ID}, FAKE_OPTIONS));
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onRemoveSection", () => {
    it("should broadcast a SECTION_DEREGISTER action with the correct data", () => {
      feed.onRemoveSection(null, FAKE_ID);
      const action = feed.store.dispatch.firstCall.args[0];
      assert.equal(action.type, "SECTION_DEREGISTER");
      assert.deepEqual(action.data, FAKE_ID);
      // Should be broadcast
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onUpdateSection", () => {
    it("should do nothing if no rows are provided", () => {
      feed.onUpdateSection(null, FAKE_ID, null);
      assert.notCalled(feed.store.dispatch);
    });
    it("should dispatch a SECTION_UPDATE action with the correct data", () => {
      feed.onUpdateSection(null, FAKE_ID, {rows: FAKE_ROWS});
      const action = feed.store.dispatch.firstCall.args[0];
      assert.equal(action.type, "SECTION_UPDATE");
      assert.deepEqual(action.data, {id: FAKE_ID, rows: FAKE_ROWS});
      // Should be not broadcast by default, so meta should not exist
      assert.notOk(action.meta);
    });
    it("should broadcast the action only if shouldBroadcast is true", () => {
      feed.onUpdateSection(null, FAKE_ID, {rows: FAKE_ROWS}, true);
      const action = feed.store.dispatch.firstCall.args[0];
      // Should be broadcast
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onAction", () => {
    it("should bind this.init to SectionsManager.INIT on INIT", () => {
      sinon.spy(SectionsManager, "once");
      feed.onAction({type: "INIT"});
      assert.calledOnce(SectionsManager.once);
      assert.calledWith(SectionsManager.once, SectionsManager.INIT, feed.init);
    });
    it("should call SectionsManager.init on action PREFS_INITIAL_VALUES", () => {
      sinon.spy(SectionsManager, "init");
      feed.onAction({type: "PREFS_INITIAL_VALUES", data: {foo: "bar"}});
      assert.calledOnce(SectionsManager.init);
      assert.calledWith(SectionsManager.init, {foo: "bar"});
    });
    it("should call SectionsManager.addBuiltInSection on suitable PREF_CHANGED events", () => {
      sinon.spy(SectionsManager, "addBuiltInSection");
      feed.onAction({type: "PREF_CHANGED", data: {name: "feeds.section.topstories.options", value: "foo"}});
      assert.calledOnce(SectionsManager.addBuiltInSection);
      assert.calledWith(SectionsManager.addBuiltInSection, "feeds.section.topstories", "foo");
    });
    it("should call SectionsManager.disableSection on SECTION_DISABLE", () => {
      sinon.spy(SectionsManager, "disableSection");
      feed.onAction({type: "SECTION_DISABLE", data: 1234});
      assert.calledOnce(SectionsManager.disableSection);
      assert.calledWith(SectionsManager.disableSection, 1234);
      SectionsManager.disableSection.restore();
    });
    it("should call SectionsManager.enableSection on SECTION_ENABLE", () => {
      sinon.spy(SectionsManager, "enableSection");
      feed.onAction({type: "SECTION_ENABLE", data: 1234});
      assert.calledOnce(SectionsManager.enableSection);
      assert.calledWith(SectionsManager.enableSection, 1234);
      SectionsManager.enableSection.restore();
    });
    it("should emit a ACTION_DISPATCHED event and forward any action in ACTIONS_TO_PROXY if there are any sections", () => {
      const spy = sinon.spy();
      const allowedActions = SectionsManager.ACTIONS_TO_PROXY;
      const disallowedActions = ["PREF_CHANGED", "OPEN_PRIVATE_WINDOW"];
      feed.init();
      SectionsManager.on(SectionsManager.ACTION_DISPATCHED, spy);
      // Make sure we start with no sections - no event should be emitted
      SectionsManager.sections.clear();
      feed.onAction({type: allowedActions[0]});
      assert.notCalled(spy);
      // Then add a section and check correct behaviour
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      for (const action of allowedActions.concat(disallowedActions)) {
        feed.onAction({type: action});
      }
      for (const action of allowedActions) {
        assert.calledWith(spy, "ACTION_DISPATCHED", action);
      }
      for (const action of disallowedActions) {
        assert.neverCalledWith(spy, "ACTION_DISPATCHED", action);
      }
    });
  });
});
