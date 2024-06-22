/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  PersistentCache: "resource://activity-stream/lib/PersistentCache.sys.mjs",
});

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://activity-stream/common/Actions.mjs";

const CACHE_KEY = "weather_feed";
const WEATHER_UPDATE_TIME = 10 * 60 * 1000; // 10 minutes
const MERINO_PROVIDER = ["accuweather"];
const MERINO_CLIENT_KEY = "HNT_WEATHER_FEED";

const PREF_WEATHER_QUERY = "weather.query";
const PREF_SHOW_WEATHER = "showWeather";
const PREF_SYSTEM_SHOW_WEATHER = "system.showWeather";

/**
 * A feature that periodically fetches weather suggestions from Merino for HNT.
 */
export class WeatherFeed {
  constructor() {
    this.loaded = false;
    this.merino = null;
    this.suggestions = [];
    this.lastUpdated = null;
    this.locationData = {};
    this.fetchTimer = null;
    this.fetchIntervalMs = 30 * 60 * 1000; // 30 minutes
    this.timeoutMS = 5000;
    this.lastFetchTimeMs = 0;
    this.fetchDelayAfterComingOnlineMs = 3000; // 3s
    this.cache = this.PersistentCache(CACHE_KEY, true);
  }

  async resetCache() {
    if (this.cache) {
      await this.cache.set("weather", {});
    }
  }

  async resetWeather() {
    await this.resetCache();
    this.suggestions = [];
    this.lastUpdated = null;
  }

  isEnabled() {
    return (
      this.store.getState().Prefs.values[PREF_SHOW_WEATHER] &&
      this.store.getState().Prefs.values[PREF_SYSTEM_SHOW_WEATHER]
    );
  }

  async init() {
    await this.loadWeather(true /* isStartup */);
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
   * This thin wrapper around the fetch call makes it easier for us to write
   * automated tests that simulate responses.
   */
  async fetchHelper(retries = 3) {
    this.restartFetchTimer();
    const weatherQuery = this.store.getState().Prefs.values[PREF_WEATHER_QUERY];
    let suggestions = [];
    let retry = 0;
    while (retry++ < retries && suggestions.length === 0) {
      try {
        suggestions = await this.merino.fetch({
          query: weatherQuery || "",
          providers: MERINO_PROVIDER,
          timeoutMs: 7000,
        });
      } catch (error) {
        // We don't need to do anything with this right now.
      }
    }

    // results from the API or empty array if null
    this.suggestions = suggestions ?? [];
  }

  async fetch(isStartup) {
    // Keep a handle on the `MerinoClient` instance that exists at the start of
    // this fetch. If fetching stops or this `Weather` instance is uninitialized
    // during the fetch, `#merino` will be nulled, and the fetch should stop. We
    // can compare `merino` to `this.merino` to tell when this occurs.
    if (!this.merino) {
      this.merino = await this.MerinoClient(MERINO_CLIENT_KEY);
    }

    await this.fetchHelper();

    if (this.suggestions.length) {
      const hasLocationData =
        !this.store.getState().Prefs.values[PREF_WEATHER_QUERY];
      this.lastUpdated = this.Date().now();
      await this.cache.set("weather", {
        suggestions: this.suggestions,
        lastUpdated: this.lastUpdated,
      });

      // only calls to merino without the query parameter would return the location data (and only city name)
      if (hasLocationData && this.suggestions.length) {
        const [data] = this.suggestions;
        this.locationData = {
          city: data.city_name,
          adminArea: "",
          country: "",
        };
        await this.cache.set("locationData", this.locationData);
      }
    }

    this.update(isStartup);
  }

  async loadWeather(isStartup = false) {
    const cachedData = (await this.cache.get()) || {};
    const { weather, locationData } = cachedData;

    // if we have locationData in the cache set it to this.locationData so it is added to the redux store
    if (locationData?.city) {
      this.locationData = locationData;
    }
    // If we have nothing in cache, or cache has expired, we can make a fresh fetch.
    if (
      !weather?.lastUpdated ||
      !(this.Date().now() - weather.lastUpdated < WEATHER_UPDATE_TIME)
    ) {
      await this.fetch(isStartup);
    } else if (!this.lastUpdated) {
      this.suggestions = weather.suggestions;
      this.lastUpdated = weather.lastUpdated;
      this.update(isStartup);
    }
  }

  update(isStartup) {
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.WEATHER_UPDATE,
        data: {
          suggestions: this.suggestions,
          lastUpdated: this.lastUpdated,
          locationData: this.locationData,
        },
        meta: {
          isStartup,
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

  async fetchLocationAutocomplete() {
    if (!this.merino) {
      this.merino = await this.MerinoClient(MERINO_CLIENT_KEY);
    }

    const query = this.store.getState().Weather.locationSearchString;
    let response = await this.merino.fetch({
      query: query || "",
      providers: MERINO_PROVIDER,
      timeoutMs: 7000,
      otherParams: {
        request_type: "location",
      },
    });
    const data = response?.[0];
    if (data?.locations.length) {
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.WEATHER_LOCATION_SUGGESTIONS_UPDATE,
          data: data.locations,
        })
      );
    }
  }

  async onPrefChangedAction(action) {
    switch (action.data.name) {
      case PREF_WEATHER_QUERY:
        await this.fetch();
        break;
      case PREF_SHOW_WEATHER:
      case PREF_SYSTEM_SHOW_WEATHER:
        if (this.isEnabled() && action.data.value) {
          await this.loadWeather();
        } else {
          await this.resetWeather();
        }
        break;
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        if (this.isEnabled()) {
          await this.init();
        }
        break;
      case at.UNINIT:
        await this.resetWeather();
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        if (this.isEnabled()) {
          await this.loadWeather();
        }
        break;
      case at.PREF_CHANGED:
        await this.onPrefChangedAction(action);
        break;
      case at.WEATHER_LOCATION_SEARCH_UPDATE:
        await this.fetchLocationAutocomplete();
        break;
      case at.WEATHER_LOCATION_DATA_UPDATE:
        // check that data is formatted correctly before adding to cache
        if (action.data.city) {
          await this.cache.set("locationData", {
            city: action.data.city,
            adminName: action.data.adminName,
            country: action.data.country,
          });
          this.locationData = action.data;
        }
        break;
    }
  }
}

/**
 * Creating a thin wrapper around MerinoClient, PersistentCache, and Date.
 * This makes it easier for us to write automated tests that simulate responses.
 */
WeatherFeed.prototype.MerinoClient = (...args) => {
  return new lazy.MerinoClient(...args);
};
WeatherFeed.prototype.PersistentCache = (...args) => {
  return new lazy.PersistentCache(...args);
};
WeatherFeed.prototype.Date = () => {
  return Date;
};
