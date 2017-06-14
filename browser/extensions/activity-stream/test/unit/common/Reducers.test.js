const {reducers, INITIAL_STATE} = require("common/Reducers.jsm");
const {TopSites, App, Prefs} = reducers;
const {actionTypes: at} = require("common/Actions.jsm");

describe("Reducers", () => {
  describe("App", () => {
    it("should return the initial state", () => {
      const nextState = App(undefined, {type: "FOO"});
      assert.equal(nextState, INITIAL_STATE.App);
    });
    it("should not set initialized to true on INIT", () => {
      const nextState = App(undefined, {type: "INIT"});
      assert.propertyVal(nextState, "initialized", true);
    });
    it("should set initialized, version, and locale on INIT", () => {
      const action = {type: "INIT", data: {version: "1.2.3"}};
      const nextState = App(undefined, action);
      assert.propertyVal(nextState, "version", "1.2.3");
    });
    it("should not update state for empty action.data on LOCALE_UPDATED", () => {
      const nextState = App(undefined, {type: at.LOCALE_UPDATED});
      assert.equal(nextState, INITIAL_STATE.App);
    });
    it("should set locale, strings on LOCALE_UPDATE", () => {
      const strings = {};
      const action = {type: "LOCALE_UPDATED", data: {locale: "zh-CN", strings}};
      const nextState = App(undefined, action);
      assert.propertyVal(nextState, "locale", "zh-CN");
      assert.propertyVal(nextState, "strings", strings);
    });
  });
  describe("TopSites", () => {
    it("should return the initial state", () => {
      const nextState = TopSites(undefined, {type: "FOO"});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should add top sites on TOP_SITES_UPDATED", () => {
      const newRows = [{url: "foo.com"}, {url: "bar.com"}];
      const nextState = TopSites(undefined, {type: at.TOP_SITES_UPDATED, data: newRows});
      assert.equal(nextState.rows, newRows);
    });
    it("should not update state for empty action.data on TOP_SITES_UPDATED", () => {
      const nextState = TopSites(undefined, {type: at.TOP_SITES_UPDATED});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should add screenshots for SCREENSHOT_UPDATED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {type: at.SCREENSHOT_UPDATED, data: {url: "bar.com", screenshot: "data:123"}};
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState.rows, [{url: "foo.com"}, {url: "bar.com", screenshot: "data:123"}]);
    });
    it("should not modify rows if nothing matches the url for SCREENSHOT_UPDATED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {type: at.SCREENSHOT_UPDATED, data: {url: "baz.com", screenshot: "data:123"}};
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState, oldState);
    });
    it("should bookmark an item on PLACES_BOOKMARK_ADDED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "bar.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          lastModified: 1234567
        }
      };
      const nextState = TopSites(oldState, action);
      const newRow = nextState.rows[1];
      // new row has bookmark data
      assert.equal(newRow.url, action.data.url);
      assert.equal(newRow.bookmarkGuid, action.data.bookmarkGuid);
      assert.equal(newRow.bookmarkTitle, action.data.bookmarkTitle);
      assert.equal(newRow.bookmarkDateCreated, action.data.lastModified);

      // old row is unchanged
      assert.equal(nextState.rows[0], oldState.rows[0]);
    });
    it("should remove a bookmark on PLACES_BOOKMARK_REMOVED", () => {
      const oldState = {
        rows: [{url: "foo.com"}, {
          url: "bar.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          lastModified: 123456
        }]
      };
      const action = {type: at.PLACES_BOOKMARK_REMOVED, data: {url: "bar.com"}};
      const nextState = TopSites(oldState, action);
      const newRow = nextState.rows[1];
      // new row no longer has bookmark data
      assert.equal(newRow.url, oldState.rows[1].url);
      assert.isUndefined(newRow.bookmarkGuid);
      assert.isUndefined(newRow.bookmarkTitle);
      assert.isUndefined(newRow.bookmarkDateCreated);

      // old row is unchanged
      assert.deepEqual(nextState.rows[0], oldState.rows[0]);
    });
    it("should remove a link on PLACES_LINK_BLOCKED and PLACES_LINK_DELETED", () => {
      const events = [at.PLACES_LINK_BLOCKED, at.PLACES_LINK_DELETED];
      events.forEach(event => {
        const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
        const action = {type: event, data: {url: "bar.com"}};
        const nextState = TopSites(oldState, action);
        assert.deepEqual(nextState.rows, [{url: "foo.com"}]);
      });
    });
  });
  describe("Prefs", () => {
    function prevState(custom = {}) {
      return Object.assign({}, INITIAL_STATE.Prefs, custom);
    }
    it("should have the correct initial state", () => {
      const state = Prefs(undefined, {});
      assert.deepEqual(state, INITIAL_STATE.Prefs);
    });
    describe("PREFS_INITIAL_VALUES", () => {
      it("should return a new object", () => {
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: {}});
        assert.notEqual(INITIAL_STATE.Prefs, state, "should not modify INITIAL_STATE");
      });
      it("should set initalized to true", () => {
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: {}});
        assert.isTrue(state.initialized);
      });
      it("should set .values", () => {
        const newValues = {foo: 1, bar: 2};
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: newValues});
        assert.equal(state.values, newValues);
      });
    });
    describe("PREF_CHANGED", () => {
      it("should return a new Prefs object", () => {
        const state = Prefs(undefined, {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.notEqual(INITIAL_STATE.Prefs, state, "should not modify INITIAL_STATE");
      });
      it("should set the changed pref", () => {
        const state = Prefs(prevState({foo: 1}), {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.equal(state.values.foo, 2);
      });
      it("should return a new .pref object instead of mutating", () => {
        const oldState = prevState({foo: 1});
        const state = Prefs(oldState, {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.notEqual(oldState.values, state.values);
      });
    });
  });
});
