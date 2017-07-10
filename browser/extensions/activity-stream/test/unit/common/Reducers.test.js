const {reducers, INITIAL_STATE, insertPinned} = require("common/Reducers.jsm");
const {TopSites, App, Prefs, Dialog} = reducers;
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
    it("should insert pinned links on PINNED_SITES_UPDATED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {type: at.PINNED_SITES_UPDATED, data: [{url: "baz.com", title: "baz"}]};
      const nextState = TopSites(oldState, action);
      console.log(nextState.rows);
      assert.deepEqual(nextState.rows, [{url: "baz.com", title: "baz", isPinned: true, pinIndex: 0, pinTitle: "baz"}, {url: "foo.com"}, {url: "bar.com"}]);
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
  describe("Dialog", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(INITIAL_STATE.Dialog, Dialog(undefined, {type: "non_existent"}));
    });
    it("should toggle visible to true on DIALOG_OPEN", () => {
      const action = {type: at.DIALOG_OPEN};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isTrue(nextState.visible);
    });
    it("should pass url data on DIALOG_OPEN", () => {
      const action = {type: at.DIALOG_OPEN, data: "some url"};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.equal(nextState.data, action.data);
    });
    it("should toggle visible to false on DIALOG_CANCEL", () => {
      const action = {type: at.DIALOG_CANCEL, data: "some url"};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isFalse(nextState.visible);
    });
    it("should return inital state on DELETE_HISTORY_URL", () => {
      const action = {type: at.DELETE_HISTORY_URL};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);

      assert.deepEqual(INITIAL_STATE.Dialog, nextState);
    });
  });
  describe("#insertPinned", () => {
    let links;

    beforeEach(() => {
      links =  new Array(12).fill(null).map((v, i) => ({url: `site${i}.com`}));
    });

    it("should place pinned links where they belong", () => {
      const pinned = [
        {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"},
        {"url": "http://example.com", "title": "example"}
      ];
      const result = insertPinned(links, pinned);
      for (let index of [0, 1]) {
        assert.equal(result[index].url, pinned[index].url);
        assert.ok(result[index].isPinned);
        assert.equal(result[index].pinTitle, pinned[index].title);
        assert.equal(result[index].pinIndex, index);
      }
      assert.deepEqual(result.slice(2), links);
    });
    it("should handle empty slots in the pinned list", () => {
      const pinned = [
        null,
        {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"},
        null,
        null,
        {"url": "http://example.com", "title": "example"}
      ];
      const result = insertPinned(links, pinned);
      for (let index of [1, 4]) {
        assert.equal(result[index].url, pinned[index].url);
        assert.ok(result[index].isPinned);
        assert.equal(result[index].pinTitle, pinned[index].title);
        assert.equal(result[index].pinIndex, index);
      }
      result.splice(4, 1);
      result.splice(1, 1);
      assert.deepEqual(result, links);
    });
    it("should handle a pinned site past the end of the list of links", () => {
      const pinned = [];
      pinned[11] = {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"};
      const result = insertPinned([], pinned);
      assert.equal(result[11].url, pinned[11].url);
      assert.isTrue(result[11].isPinned);
      assert.equal(result[11].pinTitle, pinned[11].title);
      assert.equal(result[11].pinIndex, 11);
    });
    it("should unpin previously pinned links no longer in the pinned list", () => {
      const pinned = [];
      links[2].isPinned = true;
      links[2].pinTitle = "pinned site";
      links[2].pinIndex = 2;
      const result = insertPinned(links, pinned);
      assert.notProperty(result[2], "isPinned");
      assert.notProperty(result[2], "pinTitle");
      assert.notProperty(result[2], "pinIndex");
    });
    it("should handle a link present in both the links and pinned list", () => {
      const pinned = [links[7]];
      const result = insertPinned(links, pinned);
      assert.equal(links.length, result.length);
    });
  });
});
