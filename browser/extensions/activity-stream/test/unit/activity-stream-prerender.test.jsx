const prerender = require("content-src/activity-stream-prerender");
const {prerenderStore} = prerender;
const {PrerenderData} = require("common/PrerenderData.jsm");

describe("prerenderStore", () => {
  it("should create a store", () => {
    const store = prerenderStore();

    assert.isFunction(store.getState);
  });
  it("should start uninitialized", () => {
    const store = prerenderStore();

    const state = store.getState();
    assert.equal(state.App.initialized, false);
  });
  it("should set the right locale, strings, and text direction", () => {
    const strings = {foo: "foo"};

    const store = prerenderStore("en-FOO", strings);

    const state = store.getState();
    assert.equal(state.App.locale, "en-FOO");
    assert.equal(state.App.strings, strings);
    assert.equal(state.App.textDirection, "ltr");
  });
  it("should add the right initial prefs", () => {
    const store = prerenderStore();

    const state = store.getState();
    assert.equal(state.Prefs.values, PrerenderData.initialPrefs);
  });
  it("should add TopStories as the first section", () => {
    const store = prerenderStore();

    const state = store.getState();
    // TopStories
    const firstSection = state.Sections[0];
    assert.equal(firstSection.id, "topstories");
    // it should start uninitialized
    assert.equal(firstSection.initialized, false);
  });
});

describe("prerender", () => {
  it("should set the locale and get the right strings of whatever is passed in", () => {
    const {store} = prerender("en-US");

    const state = store.getState();
    assert.equal(state.App.locale, "en-US");
    assert.equal(state.App.strings.newtab_page_title, "New Tab");
  });
  it("should throw if an unknown locale is passed in", () => {
    assert.throws(() => prerender("en-FOO"));
  });
  it("should set the locale to en-PRERENDER and have empty strings if no locale is passed in", () => {
    const {store} = prerender();

    const state = store.getState();
    assert.equal(state.App.locale, "en-PRERENDER");
    assert.equal(state.App.strings.newtab_page_title, " ");
  });
  // # TODO: Remove when #3370 is resolved.
  it("should render a real English string for search_web_placeholder", () => {
    const {store} = prerender();

    const state = store.getState();
    assert.equal(state.App.strings.search_web_placeholder, "Search the Web");
  });
});
