import {THEME_UPDATE_EVENT, ThemeFeed} from "lib/ThemeFeed.jsm";
import {actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";

describe("ThemeFeed", () => {
  let feed;
  let globals;
  let sandbox;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    sandbox.spy(global.Services.obs, "addObserver");
    sandbox.spy(global.Services.obs, "removeObserver");
    feed = new ThemeFeed();
    feed.store = {getState() { return {}; }, dispatch() {}};
  });
  afterEach(() => {
    globals.restore();
  });
  it("should create a ThemeFeed", () => {
    assert.instanceOf(feed, ThemeFeed);
  });
  it("should subscribe to THEME_UPDATE_EVENT on init", () => {
    sandbox.stub(feed, "updateTheme");
    feed.init();
    assert.calledWith(global.Services.obs.addObserver, feed, THEME_UPDATE_EVENT);
  });
  it("should unsubscribe to THEME_UPDATE_EVENT on uninit", () => {
    feed.uninit();
    assert.calledWith(global.Services.obs.removeObserver, feed, THEME_UPDATE_EVENT);
  });
  it("should call updateTheme with current theme on init", () => {
    const theme = {"id": "theme-id", "textcolor": "black", "accentcolor": "white"};
    global.LightweightThemeManager.currentThemeForDisplay = theme;
    sandbox.stub(feed, "updateTheme");
    feed.init();
    assert.calledWith(feed.updateTheme, theme);
  });
  it("should call updateTheme on THEME_UPDATE_EVENT", () => {
    const theme = {"id": "theme-id", "textcolor": "black", "accentcolor": "white"};
    sandbox.stub(feed, "updateTheme");
    feed.observe("subject", THEME_UPDATE_EVENT, JSON.stringify(theme));
    assert.calledWith(feed.updateTheme, theme);
  });
  it("should NOT call updateTheme on an event that isn't THEME_UPDATE_EVENT", () => {
    sandbox.stub(feed, "updateTheme");
    feed.observe("subject", "foo-bar-baz-event");
    assert.notCalled(feed.updateTheme);
  });
  describe("#updateTheme", () => {
    it("should switch to dark theme if the current theme is the builtin Dark theme", () => {
      const dispatch = sinon.spy();
      feed.store.dispatch = dispatch;
      feed.updateTheme({id: "firefox-compact-dark@mozilla.org"});
      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].data.className, "dark-theme");
    });
    it("should switch to default theme if the current theme has an id that isn't Dark theme", () => {
      const dispatch = sinon.spy();
      feed.store.dispatch = dispatch;
      feed.updateTheme({id: "firefox-compact-light@mozilla.org"});
      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].data.className, "");
    });
    it("should switch to default theme if the current theme has an undefined id", () => {
      const dispatch = sinon.spy();
      feed.store.dispatch = dispatch;
      feed.updateTheme({id: undefined});
      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].data.className, "");
    });
    it("should switch to default theme if the current theme is undefined (Default)", () => {
      const dispatch = sinon.spy();
      feed.store.dispatch = dispatch;
      feed.updateTheme(undefined);
      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].data.className, "");
    });
    it("should not dipatch THEME_UPDATE event if the theme applies to a specific window", () => {
      const dispatch = sinon.spy();
      feed.store.dispatch = dispatch;
      feed.updateTheme({window: 1});
      assert.notCalled(dispatch);
    });
  });
  describe("#onAction", () => {
    it("should init on INIT", async () => {
      sandbox.stub(feed, "init");
      feed.onAction({type: at.INIT});
      assert.calledOnce(feed.init);
    });
    it("should uninit on UNINIT", async () => {
      sandbox.stub(feed, "uninit");
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(feed.uninit);
    });
  });
});
