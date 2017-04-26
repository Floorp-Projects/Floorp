const {reducers, INITIAL_STATE} = require("common/Reducers.jsm");
const {TopSites, Search} = reducers;
const {actionTypes: at} = require("common/Actions.jsm");

describe("Reducers", () => {
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
  });
  describe("Search", () => {
    it("should return the initial state", () => {
      const nextState = Search(undefined, {type: "FOO"});
      assert.equal(nextState, INITIAL_STATE.Search);
    });
    it("should not update state for empty action.data on Search", () => {
      const nextState = Search(undefined, {type: at.SEARCH_STATE_UPDATED});
      assert.equal(nextState, INITIAL_STATE.Search);
    });
    it("should update the current engine and the engines on SEARCH_STATE_UPDATED", () => {
      const newEngine = {name: "Google", iconBuffer: "icon.ico"};
      const nextState = Search(undefined, {type: at.SEARCH_STATE_UPDATED, data: {currentEngine: newEngine, engines: [newEngine]}});
      assert.equal(nextState.currentEngine.name, newEngine.name);
      assert.equal(nextState.currentEngine.icon, newEngine.icon);
      assert.equal(nextState.engines[0].name, newEngine.name);
      assert.equal(nextState.engines[0].icon, newEngine.icon);
    });
  });
});
