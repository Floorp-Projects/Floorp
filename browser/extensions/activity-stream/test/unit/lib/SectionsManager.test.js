"use strict";
const {SectionsFeed, SectionsManager} = require("lib/SectionsManager.jsm");
const {EventEmitter} = require("test/unit/utils");
const {MAIN_MESSAGE_TYPE, CONTENT_MESSAGE_TYPE} = require("common/Actions.jsm");

const FAKE_ID = "FAKE_ID";
const FAKE_OPTIONS = {icon: "FAKE_ICON", title: "FAKE_TITLE"};
const FAKE_ROWS = [{url: "1"}, {url: "2"}, {"url": "3"}];

afterEach(() => {
  // Redecorate SectionsManager to remove any listeners that have been added
  EventEmitter.decorate(SectionsManager);
});

describe("SectionsManager", () => {
  it("should be initialised with .initialized == false", () => {
    assert.notOk(SectionsManager.initialized);
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
  describe("#updateRows", () => {
    it("should emit an UPDATE_ROWS event with correct arguments", () => {
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_ROWS, spy);
      SectionsManager.updateRows(FAKE_ID, FAKE_ROWS, true);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.UPDATE_ROWS, FAKE_ID, FAKE_ROWS, true);
    });
    it("should do nothing if the section doesn't exist", () => {
      SectionsManager.removeSection(FAKE_ID);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_ROWS, spy);
      SectionsManager.updateRows(FAKE_ID, FAKE_ROWS, true);
      assert.notCalled(spy);
    });
  });
});

describe("SectionsFeed", () => {
  let feed;

  beforeEach(() => {
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
        [SectionsManager.UPDATE_ROWS, feed.onUpdateRows]
      ]) {
        assert.calledWith(SectionsManager.on, event, listener);
      }
    });
    it("should emit an INIT event and set SectionsManager.initialized to true", () => {
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.INIT, spy);
      feed.init();
      assert.calledOnce(spy);
      assert.ok(SectionsManager.initialized);
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
        [SectionsManager.UPDATE_ROWS, feed.onUpdateRows]
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
  describe("#onUpdateRows", () => {
    it("should do nothing if no rows are provided", () => {
      feed.onUpdateRows(null, FAKE_ID, null);
      assert.notCalled(feed.store.dispatch);
    });
    it("should dispatch a SECTION_ROWS_UPDATE action with the correct data", () => {
      feed.onUpdateRows(null, FAKE_ID, FAKE_ROWS);
      const action = feed.store.dispatch.firstCall.args[0];
      assert.equal(action.type, "SECTION_ROWS_UPDATE");
      assert.deepEqual(action.data, {id: FAKE_ID, rows: FAKE_ROWS});
      // Should be not broadcast by default, so meta should not exist
      assert.notOk(action.meta);
    });
    it("should broadcast the action only if shouldBroadcast is true", () => {
      feed.onUpdateRows(null, FAKE_ID, FAKE_ROWS, true);
      const action = feed.store.dispatch.firstCall.args[0];
      // Should be broadcast
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onAction", () => {
    it("should call init() on action INIT", () => {
      sinon.spy(feed, "init");
      feed.onAction({type: "INIT"});
      assert.calledOnce(feed.init);
    });
    it("should emit a ACTION_DISPATCHED event and forward any action in ACTIONS_TO_PROXY if there are any sections", () => {
      const spy = sinon.spy();
      const allowedActions = SectionsManager.ACTIONS_TO_PROXY;
      const disallowedActions = ["PREF_CHANGED", "OPEN_PRIVATE_WINDOW"];
      SectionsManager.on(SectionsManager.ACTION_DISPATCHED, spy);
      // Make sure we start with no sections - no event should be emitted
      assert.equal(SectionsManager.sections.size, 0);
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
