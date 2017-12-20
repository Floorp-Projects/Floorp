import {prerender, prerenderStore} from "content-src/activity-stream-prerender";
import {PrerenderData} from "common/PrerenderData.jsm";

const messages = require("data/locales.json")["en-US"]; // eslint-disable-line import/no-commonjs

describe("prerenderStore", () => {
  it("should start uninitialized", () => {
    const store = prerenderStore();

    const state = store.getState();
    assert.equal(state.App.initialized, false);
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
  it("should provide initial rendered state", () => {
    const {store} = prerender("en-US", messages);

    const state = store.getState();
    assert.equal(state.App.initialized, false);
  });
});
