/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WeatherFeed } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/WeatherFeed.sys.mjs"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  MerinoTestUtils: "resource://testing-common/MerinoTestUtils.sys.mjs",
});

const { WEATHER_SUGGESTION } = MerinoTestUtils;

const WEATHER_ENABLED = "browser.newtabpage.activity-stream.showWeather";
const SYS_WEATHER_ENABLED =
  "browser.newtabpage.activity-stream.system.showWeather";

add_task(async function test_construction() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(WeatherFeed.prototype, "PersistentCache").returns({
    set: () => {},
    get: () => {},
  });

  let feed = new WeatherFeed();

  info("WeatherFeed constructor should create initial values");

  Assert.ok(feed, "Could construct a WeatherFeed");
  Assert.ok(feed.loaded === false, "WeatherFeed is not loaded");
  Assert.ok(feed.merino === null, "merino is initialized as null");
  Assert.ok(
    feed.suggestions.length === 0,
    "suggestions is initialized as a array with length of 0"
  );
  Assert.ok(feed.fetchTimer === null, "fetchTimer is initialized as null");
  sandbox.restore();
});

add_task(async function test_onAction_INIT() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(WeatherFeed.prototype, "MerinoClient").returns({
    get: () => [WEATHER_SUGGESTION],
    on: () => {},
  });
  sandbox.stub(WeatherFeed.prototype, "PersistentCache").returns({
    set: () => {},
    get: () => {},
  });
  const dateNowTestValue = 1;
  sandbox.stub(WeatherFeed.prototype, "Date").returns({
    now: () => dateNowTestValue,
  });

  let feed = new WeatherFeed();
  let locationData = {
    city: "testcity",
    adminArea: "",
    country: "",
  };

  Services.prefs.setBoolPref(WEATHER_ENABLED, true);
  Services.prefs.setBoolPref(SYS_WEATHER_ENABLED, true);

  sandbox.stub(feed, "isEnabled").returns(true);

  sandbox.stub(feed, "fetchHelper");
  feed.suggestions = [WEATHER_SUGGESTION];
  feed.locationData = locationData;
  feed.store = {
    dispatch: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      Prefs: {
        values: {
          "weather.query": "348794",
        },
      },
    },
  };

  info("WeatherFeed.onAction INIT should initialize Weather");

  await feed.onAction({
    type: at.INIT,
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      ac.BroadcastToContent({
        type: at.WEATHER_UPDATE,
        data: {
          suggestions: [WEATHER_SUGGESTION],
          lastUpdated: dateNowTestValue,
          locationData,
        },
        meta: {
          isStartup: true,
        },
      })
    )
  );
  Services.prefs.clearUserPref(WEATHER_ENABLED);
  sandbox.restore();
});
