const prerender = require("content-src/activity-stream-prerender");
const {prerenderStore} = prerender;
const {PrerenderData} = require("common/PrerenderData.jsm");

describe("prerenderStore", () => {
  it("should require a locale", () => {
    assert.throws(() => prerenderStore());
  });
  it("should start uninitialized", () => {
    const store = prerenderStore("en-FOO");

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
    const store = prerenderStore("en-FOO");

    const state = store.getState();
    assert.equal(state.Prefs.values, PrerenderData.initialPrefs);
  });
  it("should add TopStories as the first section", () => {
    const store = prerenderStore("en-FOO");

    const state = store.getState();
    // TopStories
    const firstSection = state.Sections[0];
    assert.equal(firstSection.id, "topstories");
    // it should start uninitialized
    assert.equal(firstSection.initialized, false);
  });
});

describe("prerender", () => {
  it("should require a locale", () => {
    assert.throws(() => prerender());
  });
  it("should set the locale and strings of whatever is passed in", () => {
    const strings = {newtab_page_title: "New Tab"};
    const {store} = prerender("en-US", strings);

    const state = store.getState();
    assert.equal(state.App.locale, "en-US");
    assert.equal(state.App.strings.newtab_page_title, "New Tab");
  });
  it("should set the direction based on locale", () => {
    const {store} = prerender("en-US");

    const state = store.getState();
    assert.equal(state.App.textDirection, "ltr");
  });
  it("should support direction for rtl locales", () => {
    const {store} = prerender("ar");

    const state = store.getState();
    assert.equal(state.App.textDirection, "rtl");
  });
});
