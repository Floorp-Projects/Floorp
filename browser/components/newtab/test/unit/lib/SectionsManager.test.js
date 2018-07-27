"use strict";
import {actionCreators as ac, actionTypes as at, CONTENT_MESSAGE_TYPE, MAIN_MESSAGE_TYPE, PRELOAD_MESSAGE_TYPE} from "common/Actions.jsm";
import {EventEmitter, GlobalOverrider} from "test/unit/utils";
import {SectionsFeed, SectionsManager} from "lib/SectionsManager.jsm";

const FAKE_ID = "FAKE_ID";
const FAKE_OPTIONS = {icon: "FAKE_ICON", title: "FAKE_TITLE"};
const FAKE_ROWS = [{url: "1.example.com", type: "bookmark"}, {url: "2.example.com", type: "pocket"}, {url: "3.example.com", type: "history"}];
const FAKE_TRENDING_ROWS =  [{url: "bar", type: "trending"}];
const FAKE_URL = "2.example.com";
const FAKE_CARD_OPTIONS = {title: "Some fake title"};

describe("SectionsManager", () => {
  let globals;
  let fakeServices;
  let fakePlacesUtils;
  let sandbox;
  let storage;

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    globals = new GlobalOverrider();
    fakeServices = {prefs: {getBoolPref: sandbox.stub(), addObserver: sandbox.stub(), removeObserver: sandbox.stub()}};
    fakePlacesUtils = {history: {update: sinon.stub(), insert: sinon.stub()}};
    globals.set({
      Services: fakeServices,
      PlacesUtils: fakePlacesUtils
    });
    // Redecorate SectionsManager to remove any listeners that have been added
    EventEmitter.decorate(SectionsManager);
    storage = {
      get: sandbox.stub().resolves(),
      set: sandbox.stub().resolves()
    };
  });

  afterEach(() => {
    globals.restore();
    sandbox.restore();
  });

  describe("#init", () => {
    it("should initialise the sections map with the built in sections", async () => {
      SectionsManager.sections.clear();
      SectionsManager.initialized = false;
      await SectionsManager.init({}, storage);
      assert.equal(SectionsManager.sections.size, 2);
      assert.ok(SectionsManager.sections.has("topstories"));
      assert.ok(SectionsManager.sections.has("highlights"));
    });
    it("should set .initialized to true", async () => {
      SectionsManager.sections.clear();
      SectionsManager.initialized = false;
      await SectionsManager.init({}, storage);
      assert.ok(SectionsManager.initialized);
    });
    it("should add observer for context menu prefs", async () => {
      SectionsManager.CONTEXT_MENU_PREFS = {"MENU_ITEM": "MENU_ITEM_PREF"};
      await SectionsManager.init({}, storage);
      assert.calledOnce(fakeServices.prefs.addObserver);
      assert.calledWith(fakeServices.prefs.addObserver, "MENU_ITEM_PREF", SectionsManager);
    });
    it("should save the reference to `storage` passed in", async () => {
      await SectionsManager.init({}, storage);

      assert.equal(SectionsManager._storage, storage);
    });
  });
  describe("#uninit", () => {
    it("should remove observer for context menu prefs", () => {
      SectionsManager.CONTEXT_MENU_PREFS = {"MENU_ITEM": "MENU_ITEM_PREF"};
      SectionsManager.initialized = true;
      SectionsManager.uninit();
      assert.calledOnce(fakeServices.prefs.removeObserver);
      assert.calledWith(fakeServices.prefs.removeObserver, "MENU_ITEM_PREF", SectionsManager);
      assert.isFalse(SectionsManager.initialized);
    });
  });
  describe("#addBuiltInSection", () => {
    it("should not report an error if options is undefined", async () => {
      globals.sandbox.spy(global.Cu, "reportError");
      SectionsManager._storage.get = sandbox.stub().returns(Promise.resolve());
      await SectionsManager.addBuiltInSection("feeds.section.topstories", undefined);

      assert.notCalled(Cu.reportError);
    });
    it("should report an error if options is malformed", async () => {
      globals.sandbox.spy(global.Cu, "reportError");
      SectionsManager._storage.get = sandbox.stub().returns(Promise.resolve());
      await SectionsManager.addBuiltInSection("feeds.section.topstories", "invalid");

      assert.calledOnce(Cu.reportError);
    });
    it("should not throw if the indexedDB operation fails", async () => {
      globals.sandbox.spy(global.Cu, "reportError");
      storage.get.returns(new Error());
      SectionsManager._storage = storage;

      try {
        await SectionsManager.addBuiltInSection("feeds.section.topstories");
      } catch (e) {
        assert.fail();
      }

      assert.calledOnce(storage.get);
      assert.calledOnce(Cu.reportError);
    });
  });
  describe("#updateSectionPrefs", () => {
    it("should update the collapsed value of the section", async () => {
      sandbox.stub(SectionsManager, "updateSection");
      let topstories = SectionsManager.sections.get("topstories");
      assert.isFalse(topstories.pref.collapsed);

      await SectionsManager.updateSectionPrefs("topstories", {collapsed: true});
      topstories = SectionsManager.sections.get("topstories");

      assert.isTrue(SectionsManager.updateSection.args[0][1].pref.collapsed);
    });
    it("should ignore invalid ids", async () => {
      sandbox.stub(SectionsManager, "updateSection");
      await SectionsManager.updateSectionPrefs("foo", {collapsed: true});

      assert.notCalled(SectionsManager.updateSection);
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
    it("should emit an ENABLE_SECTION event", () => {
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.ENABLE_SECTION, spy);
      SectionsManager.enableSection(FAKE_ID);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.ENABLE_SECTION, FAKE_ID);
    });
  });
  describe("#disableSection", () => {
    it("should call updateSection with {enabled: false, rows: [], initialized: false}", () => {
      sinon.spy(SectionsManager, "updateSection");
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      SectionsManager.disableSection(FAKE_ID);
      assert.calledOnce(SectionsManager.updateSection);
      assert.calledWith(SectionsManager.updateSection, FAKE_ID, {enabled: false, rows: [], initialized: false}, true);
      SectionsManager.updateSection.restore();
    });
    it("should emit a DISABLE_SECTION event", () => {
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.DISABLE_SECTION, spy);
      SectionsManager.disableSection(FAKE_ID);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.DISABLE_SECTION, FAKE_ID);
    });
  });
  describe("#updateSection", () => {
    it("should emit an UPDATE_SECTION event with correct arguments", () => {
      SectionsManager.addSection(FAKE_ID, FAKE_OPTIONS);
      const spy = sinon.spy();
      const dedupeConfigurations = [{id: "topstories", dedupeFrom: ["highlights"]}];
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.updateSection(FAKE_ID, {rows: FAKE_ROWS}, true);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.UPDATE_SECTION, FAKE_ID, {rows: FAKE_ROWS, dedupeConfigurations}, true);
    });
    it("should do nothing if the section doesn't exist", () => {
      SectionsManager.removeSection(FAKE_ID);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.updateSection(FAKE_ID, {rows: FAKE_ROWS}, true);
      assert.notCalled(spy);
    });
    it("should update all sections", () => {
      SectionsManager.sections.clear();
      const updateSectionOrig = SectionsManager.updateSection;
      SectionsManager.updateSection = sinon.spy();

      SectionsManager.addSection("ID1", {title: "FAKE_TITLE_1"});
      SectionsManager.addSection("ID2", {title: "FAKE_TITLE_2"});
      SectionsManager.updateSections();

      assert.calledTwice(SectionsManager.updateSection);
      assert.calledWith(SectionsManager.updateSection, "ID1", {title: "FAKE_TITLE_1"}, true);
      assert.calledWith(SectionsManager.updateSection, "ID2", {title: "FAKE_TITLE_2"}, true);
      SectionsManager.updateSection = updateSectionOrig;
    });
    it("context menu pref change should update sections", async () => {
      let observer;
      const services = {prefs: {getBoolPref: sinon.spy(), addObserver: (pref, o) => (observer = o), removeObserver: sinon.spy()}};
      globals.set("Services", services);

      SectionsManager.updateSections = sinon.spy();
      SectionsManager.CONTEXT_MENU_PREFS = {"MENU_ITEM": "MENU_ITEM_PREF"};
      await SectionsManager.init({}, storage);
      observer.observe("", "nsPref:changed", "MENU_ITEM_PREF");

      assert.calledOnce(SectionsManager.updateSections);
    });
  });
  describe("#_addCardTypeLinkMenuOptions", () => {
    const addCardTypeLinkMenuOptionsOrig = SectionsManager._addCardTypeLinkMenuOptions;
    const contextMenuOptionsOrig = SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES;
    beforeEach(() => {
      // Add a topstories section and a highlights section, with types for each card
      SectionsManager.addSection("topstories", {FAKE_TRENDING_ROWS});
      SectionsManager.addSection("highlights", {FAKE_ROWS});
    });
    it("should only call _addCardTypeLinkMenuOptions if the section update is for highlights", () => {
      SectionsManager._addCardTypeLinkMenuOptions = sinon.spy();
      SectionsManager.updateSection("topstories", {rows: FAKE_ROWS}, false);
      assert.notCalled(SectionsManager._addCardTypeLinkMenuOptions);

      SectionsManager.updateSection("highlights", {rows: FAKE_ROWS}, false);
      assert.calledWith(SectionsManager._addCardTypeLinkMenuOptions, FAKE_ROWS);
    });
    it("should only call _addCardTypeLinkMenuOptions if the section update has rows", () => {
      SectionsManager._addCardTypeLinkMenuOptions = sinon.spy();
      SectionsManager.updateSection("highlights", {}, false);
      assert.notCalled(SectionsManager._addCardTypeLinkMenuOptions);
    });
    it("should assign the correct context menu options based on the type of highlight", () => {
      SectionsManager._addCardTypeLinkMenuOptions = addCardTypeLinkMenuOptionsOrig;

      SectionsManager.updateSection("highlights", {rows: FAKE_ROWS}, false);
      const highlights = SectionsManager.sections.get("highlights").FAKE_ROWS;

      // FAKE_ROWS was added in the following order: bookmark, pocket, history
      assert.deepEqual(highlights[0].contextMenuOptions, SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES.bookmark);
      assert.deepEqual(highlights[1].contextMenuOptions, SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES.pocket);
      assert.deepEqual(highlights[2].contextMenuOptions, SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES.history);
    });
    it("should throw an error if you are assigning a context menu to a non-existant highlight type", () => {
      globals.sandbox.spy(global.Cu, "reportError");
      SectionsManager.updateSection("highlights", {rows: [{url: "foo", type: "badtype"}]}, false);
      const highlights = SectionsManager.sections.get("highlights").rows;
      assert.calledOnce(Cu.reportError);
      assert.equal(highlights[0].contextMenuOptions, undefined);
    });
    it("should filter out context menu options that are in CONTEXT_MENU_PREFS", () => {
      const services = {prefs: {getBoolPref: o => SectionsManager.CONTEXT_MENU_PREFS[o] !== "RemoveMe", addObserver() {}, removeObserver() {}}};
      globals.set("Services", services);
      SectionsManager.CONTEXT_MENU_PREFS = {"RemoveMe": "RemoveMe"};
      SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES = {
        "bookmark": ["KeepMe", "RemoveMe"],
        "pocket": ["KeepMe", "RemoveMe"],
        "history": ["KeepMe", "RemoveMe"]
      };
      SectionsManager.updateSection("highlights", {rows: FAKE_ROWS}, false);
      const highlights = SectionsManager.sections.get("highlights").FAKE_ROWS;

      // Only keep context menu options that were not supposed to be removed based on CONTEXT_MENU_PREFS
      assert.deepEqual(highlights[0].contextMenuOptions, ["KeepMe"]);
      assert.deepEqual(highlights[1].contextMenuOptions, ["KeepMe"]);
      assert.deepEqual(highlights[2].contextMenuOptions, ["KeepMe"]);
      SectionsManager.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES = contextMenuOptionsOrig;
      globals.restore();
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
  describe("#updateSectionCard", () => {
    it("should emit an UPDATE_SECTION_CARD event with correct arguments", () => {
      SectionsManager.addSection(FAKE_ID, Object.assign({}, FAKE_OPTIONS, {rows: FAKE_ROWS}));
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION_CARD, spy);
      SectionsManager.updateSectionCard(FAKE_ID, FAKE_URL, FAKE_CARD_OPTIONS, true);
      assert.calledOnce(spy);
      assert.calledWith(spy, SectionsManager.UPDATE_SECTION_CARD, FAKE_ID,
        FAKE_URL, FAKE_CARD_OPTIONS, true);
    });
    it("should do nothing if the section doesn't exist", () => {
      SectionsManager.removeSection(FAKE_ID);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION_CARD, spy);
      SectionsManager.updateSectionCard(FAKE_ID, FAKE_URL, FAKE_CARD_OPTIONS, true);
      assert.notCalled(spy);
    });
  });
  describe("#removeSectionCard", () => {
    it("should dispatch an SECTION_UPDATE action in which cards corresponding to the given url are removed", () => {
      const rows = [{url: "foo.com"}, {url: "bar.com"}];

      SectionsManager.addSection(FAKE_ID, Object.assign({}, FAKE_OPTIONS, {rows}));
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.removeSectionCard(FAKE_ID, "foo.com");

      assert.calledOnce(spy);
      assert.equal(spy.firstCall.args[1], FAKE_ID);
      assert.deepEqual(spy.firstCall.args[2].rows, [{url: "bar.com"}]);
    });
    it("should do nothing if the section doesn't exist", () => {
      SectionsManager.removeSection(FAKE_ID);
      const spy = sinon.spy();
      SectionsManager.on(SectionsManager.UPDATE_SECTION, spy);
      SectionsManager.removeSectionCard(FAKE_ID, "bar.com");
      assert.notCalled(spy);
    });
  });
  describe("#updateBookmarkMetadata", () => {
    beforeEach(() => {
      let rows = [{
        url: "bar",
        title: "title",
        description: "description",
        image: "image",
        type: "trending"
      }];
      SectionsManager.addSection("topstories", {rows});
      // Simulate 2 sections.
      rows = [{
        url: "foo",
        title: "title",
        description: "description",
        image: "image",
        type: "bookmark"
      }];
      SectionsManager.addSection("highlights", {rows});
    });

    it("shouldn't call PlacesUtils if URL is not in topstories", () => {
      SectionsManager.updateBookmarkMetadata({url: "foo"});

      assert.notCalled(fakePlacesUtils.history.update);
    });
    it("should call PlacesUtils.history.update", () => {
      SectionsManager.updateBookmarkMetadata({url: "bar"});

      assert.calledOnce(fakePlacesUtils.history.update);
      assert.calledWithExactly(fakePlacesUtils.history.update, {
        url: "bar",
        title: "title",
        description: "description",
        previewImageURL: "image"
      });
    });
    it("should call PlacesUtils.history.insert", () => {
      SectionsManager.updateBookmarkMetadata({url: "bar"});

      assert.calledOnce(fakePlacesUtils.history.insert);
      assert.calledWithExactly(fakePlacesUtils.history.insert, {
        url: "bar",
        title: "title",
        visits: [{}]
      });
    });
  });
});

describe("SectionsFeed", () => {
  let feed;
  let sandbox;
  let storage;

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    SectionsManager.sections.clear();
    SectionsManager.initialized = false;
    storage = {
      get: sandbox.stub().resolves(),
      set: sandbox.stub().resolves()
    };
    feed = new SectionsFeed();
    feed.store = {dispatch: sinon.spy()};
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      state: {
        Prefs: {
          values: {
            "sectionOrder": "topsites,topstories,highlights",
            "feeds.topsites": true
          }
        },
        Sections: [{initialized: false}]
      },
      dbStorage: {getDbTable: sandbox.stub().returns(storage)}
    };
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
      assert.callCount(SectionsManager.on, 4);
      for (const [event, listener] of [
        [SectionsManager.ADD_SECTION, feed.onAddSection],
        [SectionsManager.REMOVE_SECTION, feed.onRemoveSection],
        [SectionsManager.UPDATE_SECTION, feed.onUpdateSection],
        [SectionsManager.UPDATE_SECTION_CARD, feed.onUpdateSectionCard]
      ]) {
        assert.calledWith(SectionsManager.on, event, listener);
      }
    });
    it("should call onAddSection for any already added sections in SectionsManager", async () => {
      await SectionsManager.init({}, storage);
      assert.ok(SectionsManager.sections.has("topstories"));
      assert.ok(SectionsManager.sections.has("highlights"));
      const topstories = SectionsManager.sections.get("topstories");
      const highlights = SectionsManager.sections.get("highlights");
      sinon.spy(feed, "onAddSection");
      feed.init();
      assert.calledTwice(feed.onAddSection);
      assert.calledWith(feed.onAddSection, SectionsManager.ADD_SECTION, "topstories", topstories);
      assert.calledWith(feed.onAddSection, SectionsManager.ADD_SECTION, "highlights", highlights);
    });
  });
  describe("#uninit", () => {
    it("should unbind all listeners", () => {
      sinon.spy(SectionsManager, "off");
      feed.init();
      feed.uninit();
      assert.callCount(SectionsManager.off, 4);
      for (const [event, listener] of [
        [SectionsManager.ADD_SECTION, feed.onAddSection],
        [SectionsManager.REMOVE_SECTION, feed.onRemoveSection],
        [SectionsManager.UPDATE_SECTION, feed.onUpdateSection],
        [SectionsManager.UPDATE_SECTION_CARD, feed.onUpdateSectionCard]
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
      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, "SECTION_REGISTER");
      assert.deepEqual(action.data, Object.assign({id: FAKE_ID}, FAKE_OPTIONS));
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
    it("should prepend id to sectionOrder pref if not already included", () => {
      feed.store.state.Sections = [{id: "topstories", enabled: true}, {id: "highlights", enabled: true}];
      feed.onAddSection(null, FAKE_ID, FAKE_OPTIONS);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: `${FAKE_ID},topsites,topstories,highlights`},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
    });
  });
  describe("#onRemoveSection", () => {
    it("should broadcast a SECTION_DEREGISTER action with the correct data", () => {
      feed.onRemoveSection(null, FAKE_ID);
      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, "SECTION_DEREGISTER");
      assert.deepEqual(action.data, FAKE_ID);
      // Should be broadcast
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onUpdateSection", () => {
    it("should do nothing if no options are provided", () => {
      feed.onUpdateSection(null, FAKE_ID, null);
      assert.notCalled(feed.store.dispatch);
    });
    it("should dispatch a SECTION_UPDATE action with the correct data", () => {
      feed.onUpdateSection(null, FAKE_ID, {rows: FAKE_ROWS});
      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, "SECTION_UPDATE");
      assert.deepEqual(action.data, {id: FAKE_ID, rows: FAKE_ROWS});
      // Should be not broadcast by default, but should update the preloaded tab, so check meta
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, PRELOAD_MESSAGE_TYPE);
    });
    it("should broadcast the action only if shouldBroadcast is true", () => {
      feed.onUpdateSection(null, FAKE_ID, {rows: FAKE_ROWS}, true);
      const [action] = feed.store.dispatch.firstCall.args;
      // Should be broadcast
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#onUpdateSectionCard", () => {
    it("should do nothing if no options are provided", () => {
      feed.onUpdateSectionCard(null, FAKE_ID, FAKE_URL, null);
      assert.notCalled(feed.store.dispatch);
    });
    it("should dispatch a SECTION_UPDATE_CARD action with the correct data", () => {
      feed.onUpdateSectionCard(null, FAKE_ID, FAKE_URL, FAKE_CARD_OPTIONS);
      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, "SECTION_UPDATE_CARD");
      assert.deepEqual(action.data, {id: FAKE_ID, url: FAKE_URL, options: FAKE_CARD_OPTIONS});
      // Should be not broadcast by default, but should update the preloaded tab, so check meta
      assert.equal(action.meta.from, MAIN_MESSAGE_TYPE);
      assert.equal(action.meta.to, PRELOAD_MESSAGE_TYPE);
    });
    it("should broadcast the action only if shouldBroadcast is true", () => {
      feed.onUpdateSectionCard(null, FAKE_ID, FAKE_URL, FAKE_CARD_OPTIONS, true);
      const [action] = feed.store.dispatch.firstCall.args;
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
      assert.calledOnce(feed.store.dbStorage.getDbTable);
      assert.calledWithExactly(feed.store.dbStorage.getDbTable, "sectionPrefs");
    });
    it("should call SectionsManager.addBuiltInSection on suitable PREF_CHANGED events", () => {
      sinon.spy(SectionsManager, "addBuiltInSection");
      feed.onAction({type: "PREF_CHANGED", data: {name: "feeds.section.topstories.options", value: "foo"}});
      assert.calledOnce(SectionsManager.addBuiltInSection);
      assert.calledWith(SectionsManager.addBuiltInSection, "feeds.section.topstories", "foo");
    });
    it("should fire SECTION_OPTIONS_UPDATED on suitable PREF_CHANGED events", () => {
      feed.onAction({type: "PREF_CHANGED", data: {name: "feeds.section.topstories.options", value: "foo"}});
      assert.calledOnce(feed.store.dispatch);
      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, "SECTION_OPTIONS_CHANGED");
      assert.equal(action.data, "topstories");
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
    it("should call the feed's uninit on UNINIT", () => {
      sinon.stub(feed, "uninit");

      feed.onAction({type: "UNINIT"});

      assert.calledOnce(feed.uninit);
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
    it("should call updateBookmarkMetadata on PLACES_BOOKMARK_ADDED", () => {
      const stub = sinon.stub(SectionsManager, "updateBookmarkMetadata");

      feed.onAction({type: "PLACES_BOOKMARK_ADDED", data: {}});

      assert.calledOnce(stub);
    });
    it("should call updateSectionPrefs on UPDATE_SECTION_PREFS", () => {
      const stub = sinon.stub(SectionsManager, "updateSectionPrefs");

      feed.onAction({type: "UPDATE_SECTION_PREFS", data: {}});

      assert.calledOnce(stub);
    });
    it("should call SectionManager.removeSectionCard on WEBEXT_DISMISS", () => {
      const stub = sinon.stub(SectionsManager, "removeSectionCard");

      feed.onAction(ac.WebExtEvent(at.WEBEXT_DISMISS, {source: "Foo", url: "bar.com"}));

      assert.calledOnce(stub);
      assert.calledWith(stub, "Foo", "bar.com");
    });
    it("should call the feed's moveSection on SECTION_MOVE", () => {
      sinon.stub(feed, "moveSection");
      const id = "topsites";
      const direction = +1;
      feed.onAction({type: "SECTION_MOVE", data: {id, direction}});

      assert.calledOnce(feed.moveSection);
      assert.calledWith(feed.moveSection, id, direction);
    });
  });
  describe("#moveSection", () => {
    it("should Move Down correctly", () => {
      feed.store.state.Sections = [{id: "topstories", enabled: true}, {id: "highlights", enabled: true}];
      feed.moveSection("topsites", +1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "topstories,topsites,highlights"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
      feed.store.dispatch.reset();
      feed.moveSection("topstories", +1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "topsites,highlights,topstories"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
    });
    it("should Move Up correctly", () => {
      feed.store.state.Sections = [{id: "topstories", enabled: true}, {id: "highlights", enabled: true}];
      feed.moveSection("topstories", -1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "topstories,topsites,highlights"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
      feed.store.dispatch.reset();
      feed.moveSection("highlights", -1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "topsites,highlights,topstories"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
    });
    it("should skip over sections that aren't enabled", () => {
      feed.store.state.Sections = [{id: "topstories", enabled: false}, {id: "highlights", enabled: true}];
      feed.moveSection("highlights", -1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "highlights,topsites,topstories"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
      feed.store.dispatch.reset();
      feed.moveSection("topsites", +1);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        data: {name: "sectionOrder", value: "topstories,highlights,topsites"},
        meta: {from: "ActivityStream:Content", to: "ActivityStream:Main"},
        type: "SET_PREF"
      });
    });
  });
});
