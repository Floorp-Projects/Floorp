"use strict";
const {SearchFeed} = require("lib/SearchFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");
const fakeEngines = [{name: "Google", iconBuffer: "icon.ico"}];
describe("Search Feed", () => {
  let feed;
  let globals;
  before(() => {
    globals = new GlobalOverrider();
    globals.set("ContentSearch", {
      currentStateObj: globals.sandbox.spy(() => Promise.resolve({engines: fakeEngines, currentEngine: {}})),
      performSearch: globals.sandbox.spy((browser, searchData) => Promise.resolve({browser, searchData}))
    });
  });
  beforeEach(() => {
    feed = new SearchFeed();
    feed.store = {dispatch: sinon.spy()};
  });
  afterEach(() => globals.reset());
  after(() => globals.restore());

  it("should call get state (with true) from the content search provider on INIT", () => {
    feed.onAction({type: at.INIT});
    // calling currentStateObj with 'true' allows us to return a data uri for the
    // icon, instead of an array buffer
    assert.calledWith(global.ContentSearch.currentStateObj, true);
  });
  it("should get the the state on INIT", () => {
    sinon.stub(feed, "getState");
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.getState);
  });
  it("should add observers on INIT", () => {
    sinon.stub(feed, "addObservers");
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.addObservers);
  });
  it("should remove observers on UNINIT", () => {
    sinon.stub(feed, "removeObservers");
    feed.onAction({type: at.UNINIT});
    assert.calledOnce(feed.removeObservers);
  });
  it("should call services.obs.addObserver on INIT", () => {
    feed.onAction({type: at.INIT});
    assert.calledOnce(global.Services.obs.addObserver);
  });
  it("should call services.obs.removeObserver on UNINIT", () => {
    feed.onAction({type: at.UNINIT});
    assert.calledOnce(global.Services.obs.removeObserver);
  });
  it("should dispatch one event with the state", () => (
    feed.getState().then(() => {
      assert.calledOnce(feed.store.dispatch);
    })
  ));
  it("should perform a search on PERFORM_SEARCH", () => {
    sinon.stub(feed, "performSearch");
    feed.onAction({_target: {browser: {}}, type: at.PERFORM_SEARCH});
    assert.calledOnce(feed.performSearch);
  });
  it("should call performSearch with an action", () => {
    const action = {_target: {browser: "browser"}, data: {searchString: "hello"}};
    feed.performSearch(action._target.browser, action.data);
    assert.calledWith(global.ContentSearch.performSearch, {target: action._target.browser}, action.data);
  });
  it("should get the state if we change the search engines", () => {
    sinon.stub(feed, "getState");
    feed.observe(null, "browser-search-engine-modified", "engine-current");
    assert.calledOnce(feed.getState);
  });
  it("shouldn't get the state if it's not the right notification", () => {
    sinon.stub(feed, "getState");
    feed.observe(null, "some-other-notification", "engine-current");
    assert.notCalled(feed.getState);
  });
});
