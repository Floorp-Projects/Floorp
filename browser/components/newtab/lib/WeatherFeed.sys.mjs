/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://activity-stream/common/Actions.mjs";

const MERINO_PROVIDER = "accuweather";
const WEATHER_ENABLED = "browser.newtabpage.activity-stream.showWeather";
const WEATHER_ENABLED_SYS =
  "browser.newtabpage.activity-stream.system.showWeather";

const WEATHER_QUERY = "browser.newtabpage.activity-stream.weather.query";

/**
 * A feature that periodically fetches weather suggestions from Merino for HNT.
 */
export class WeatherFeed {
  constructor() {
    this.loaded = false;
    this.merino = null;
    this.suggestions = [];
    this.lastUpdated = null;
    this.fetchTimer = null;
    this.fetchIntervalMs = 30 * 60 * 1000; // 30 minutes
    this.timeoutMS = 5000;
    this.lastFetchTimeMs = 0;
    this.fetchDelayAfterComingOnlineMs = 3000; // 3s
  }

  isEnabled() {
    return (
      Services.prefs.getBoolPref(WEATHER_ENABLED) &&
      Services.prefs.getBoolPref(WEATHER_ENABLED_SYS)
    );
  }

  init() {
    this.fetch();
  }

  stopFetching() {
    if (!this.merino) {
      return;
    }

    lazy.clearTimeout(this.fetchTimer);
    this.merino = null;
    this.suggestions = null;
    this.fetchTimer = 0;
  }

  /**
   * This thin wrapper around lazy.MerinoClient makes it easier for us to write
   * automated tests that simulate responses.
   */
  MerinoClient(...args) {
    return new lazy.MerinoClient(...args);
  }
  /**
   * This thin wrapper around the fetch call makes it easier for us to write
   * automated tests that simulate responses.
   */
  async fetchHelper() {
    this.restartFetchTimer();
    this.lastUpdated = Date.now();
    const weatherQuery = Services.prefs.getStringPref(WEATHER_QUERY);
    let suggestions = await this.merino.fetch({
      query: weatherQuery || "",
      providers: [MERINO_PROVIDER],
      timeoutMs: 5000,
    });

    // results from the API or empty array if null
    this.suggestions = suggestions ?? [];
  }

  async fetch() {
    // Keep a handle on the `MerinoClient` instance that exists at the start of
    // this fetch. If fetching stops or this `Weather` instance is uninitialized
    // during the fetch, `#merino` will be nulled, and the fetch should stop. We
    // can compare `merino` to `this.merino` to tell when this occurs.
    this.merino = await this.MerinoClient("HNT_WEATHER_FEED");
    await this.fetchHelper();

    if (this.suggestions.length) {
      this.update();
    }
  }

  update() {
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.WEATHER_UPDATE,
        data: {
          suggestions: this.suggestions,
          lastUpdated: this.lastUpdated,
        },
      })
    );
  }

  restartFetchTimer(ms = this.fetchIntervalMs) {
    lazy.clearTimeout(this.fetchTimer);
    this.fetchTimer = lazy.setTimeout(() => {
      this.fetch();
    }, ms);
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        if (this.isEnabled()) {
          this.init();
        }
        break;
      case at.UNINIT:
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        if (this.isEnabled()) {
          await this.fetch();
        }
        break;
      case at.PREF_CHANGED:
        if (action.data.name === "weather.query") {
          await this.fetch();
        } else if (
          action.data.name === "showWeather" &&
          action.data.value &&
          this.isEnabled()
        ) {
          await this.fetch();
        }
        break;
    }
  }
}
