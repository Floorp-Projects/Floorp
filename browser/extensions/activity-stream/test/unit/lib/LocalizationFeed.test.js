"use strict";
const {LocalizationFeed} = require("lib/LocalizationFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");

const DEFAULT_LOCALE = "en-US";
const TEST_STRINGS = {
  [DEFAULT_LOCALE]: {
    foo: "Foo",
    too: "Too"
  },
  "it": {
    foo: "Bar",
    too: "Boo"
  },
  "ru": {foo: "Baz"}
};

describe("Localization Feed", () => {
  let feed;
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    feed = new LocalizationFeed();
    feed.store = {dispatch: sinon.spy()};
  });
  afterEach(() => {
    globals.restore();
  });

  it("should fetch strings on init", async () => {
    sandbox.stub(feed, "updateLocale");
    sandbox.stub(global, "fetch");
    fetch.returns(Promise.resolve({json() { return Promise.resolve(TEST_STRINGS); }}));

    await feed.init();

    assert.deepEqual(feed.allStrings, TEST_STRINGS);
    assert.calledOnce(feed.updateLocale);
  });

  describe("#updateLocale", () => {
    beforeEach(() => {
      feed.allStrings = TEST_STRINGS;
    });

    it("should dispatch with locale and strings for default", () => {
      const locale = DEFAULT_LOCALE;
      sandbox.stub(global.Services.locale, "negotiateLanguages")
        .returns([locale]);
      feed.updateLocale();

      assert.calledOnce(feed.store.dispatch);
      const arg = feed.store.dispatch.firstCall.args[0];
      assert.propertyVal(arg, "type", at.LOCALE_UPDATED);
      assert.propertyVal(arg.data, "locale", locale);
      assert.deepEqual(arg.data.strings, TEST_STRINGS[locale]);
    });
    it("should use strings for other locale", () => {
      const locale = "it";
      sandbox.stub(global.Services.locale, "negotiateLanguages")
        .returns([locale, DEFAULT_LOCALE]);

      feed.updateLocale();

      assert.calledOnce(feed.store.dispatch);
      const arg = feed.store.dispatch.firstCall.args[0];
      assert.propertyVal(arg, "type", at.LOCALE_UPDATED);
      assert.propertyVal(arg.data, "locale", locale);
      assert.deepEqual(arg.data.strings, TEST_STRINGS[locale]);
    });
    it("should use some fallback strings for partial locale", () => {
      const locale = "ru";
      sandbox.stub(global.Services.locale, "negotiateLanguages")
        .returns([locale, DEFAULT_LOCALE]);

      feed.updateLocale();

      assert.calledOnce(feed.store.dispatch);
      const arg = feed.store.dispatch.firstCall.args[0];
      assert.propertyVal(arg, "type", at.LOCALE_UPDATED);
      assert.propertyVal(arg.data, "locale", locale);
      assert.deepEqual(arg.data.strings, {
        foo: TEST_STRINGS[locale].foo,
        too: TEST_STRINGS[DEFAULT_LOCALE].too
      });
    });
    it("should use multiple fallback strings before default", () => {
      const primaryLocale = "ru";
      const secondaryLocale = "it";
      sandbox.stub(global.Services.locale, "negotiateLanguages")
        .returns([primaryLocale, secondaryLocale, DEFAULT_LOCALE]);

      feed.updateLocale();

      assert.calledOnce(feed.store.dispatch);
      const arg = feed.store.dispatch.firstCall.args[0];
      assert.propertyVal(arg, "type", at.LOCALE_UPDATED);
      assert.propertyVal(arg.data, "locale", primaryLocale);
      assert.deepEqual(arg.data.strings, {
        foo: TEST_STRINGS[primaryLocale].foo,
        too: TEST_STRINGS[secondaryLocale].too
      });
    });
    it("should use all default strings for unknown locale", () => {
      sandbox.stub(global.Services.locale, "negotiateLanguages")
        .returns([DEFAULT_LOCALE]);

      feed.updateLocale();

      assert.calledOnce(feed.store.dispatch);
      const arg = feed.store.dispatch.firstCall.args[0];
      assert.propertyVal(arg, "type", at.LOCALE_UPDATED);
      assert.propertyVal(arg.data, "locale", DEFAULT_LOCALE);
      assert.deepEqual(arg.data.strings, TEST_STRINGS[DEFAULT_LOCALE]);
    });
  });

  describe("#observe", () => {
    it("should update locale on locale change event", () => {
      sinon.stub(feed, "updateLocale");

      feed.observe(null, "intl:requested-locales-changed");

      assert.calledOnce(feed.updateLocale);
    });
    it("shouldn't update locale on other event", () => {
      sinon.stub(feed, "updateLocale");

      feed.observe(null, "some-other-notification");

      assert.notCalled(feed.updateLocale);
    });
  });

  describe("#onAction", () => {
    it("should addObserver on INIT", () => {
      const stub = sandbox.stub(global.Services.obs, "addObserver");

      feed.onAction({type: at.INIT});

      assert.calledOnce(stub);
    });
    it("should removeObserver on UNINIT", () => {
      const stub = sandbox.stub(global.Services.obs, "removeObserver");

      feed.onAction({type: at.UNINIT});

      assert.calledOnce(stub);
    });
  });
});
