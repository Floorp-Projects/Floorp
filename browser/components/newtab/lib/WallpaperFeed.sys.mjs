/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  Utils: "resource://services-settings/Utils.sys.mjs",
});

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://activity-stream/common/Actions.mjs";

const PREF_WALLPAPERS_ENABLED =
  "browser.newtabpage.activity-stream.newtabWallpapers.enabled";

const PREF_WALLPAPERS_HIGHLIGHT_SEEN_COUNTER =
  "browser.newtabpage.activity-stream.newtabWallpapers.highlightSeenCounter";

const PREF_WALLPAPERS_V2_ENABLED =
  "browser.newtabpage.activity-stream.newtabWallpapers.v2.enabled";

const WALLPAPER_REMOTE_SETTINGS_COLLECTION = "newtab-wallpapers";
const WALLPAPER_REMOTE_SETTINGS_COLLECTION_V2 = "newtab-wallpapers-v2";

export class WallpaperFeed {
  constructor() {
    this.loaded = false;
    this.wallpaperClient = null;
    this.baseAttachmentURL = "";
    this._onSync = this.onSync.bind(this);
  }

  /**
   * This thin wrapper around global.fetch makes it easier for us to write
   * automated tests that simulate responses from this fetch.
   */
  fetch(...args) {
    return fetch(...args);
  }

  /**
   * This thin wrapper around lazy.RemoteSettings makes it easier for us to write
   * automated tests that simulate responses from this fetch.
   */
  RemoteSettings(...args) {
    return lazy.RemoteSettings(...args);
  }

  async wallpaperSetup(isStartup = false) {
    const wallpapersEnabled = Services.prefs.getBoolPref(
      PREF_WALLPAPERS_ENABLED
    );

    const wallpapersV2Enabled = Services.prefs.getBoolPref(
      PREF_WALLPAPERS_V2_ENABLED
    );

    if (wallpapersEnabled || wallpapersV2Enabled) {
      if (!this.wallpaperClient) {
        // getting collection
        if (wallpapersV2Enabled) {
          this.wallpaperClient = this.RemoteSettings(
            WALLPAPER_REMOTE_SETTINGS_COLLECTION_V2
          );
        } else {
          this.wallpaperClient = this.RemoteSettings(
            WALLPAPER_REMOTE_SETTINGS_COLLECTION
          );
        }
      }

      await this.getBaseAttachment();
      this.wallpaperClient.on("sync", this._onSync);
      this.updateWallpapers(isStartup);
    }
  }

  async wallpaperTeardown() {
    if (this._onSync) {
      this.wallpaperClient?.off("sync", this._onSync);
    }
    this.loaded = false;
    this.wallpaperClient = null;
    this.baseAttachmentURL = "";
  }

  async onSync() {
    this.wallpaperTeardown();
    await this.wallpaperSetup(false /* isStartup */);
  }

  async getBaseAttachment() {
    if (!this.baseAttachmentURL) {
      const SERVER = lazy.Utils.SERVER_URL;
      const serverInfo = await (
        await this.fetch(`${SERVER}/`, {
          credentials: "omit",
        })
      ).json();
      const { base_url } = serverInfo.capabilities.attachments;
      this.baseAttachmentURL = base_url;
    }
  }

  async updateWallpapers(isStartup = false) {
    // retrieving all records in collection
    const records = await this.wallpaperClient.get();
    if (!records?.length) {
      return;
    }

    if (!this.baseAttachmentURL) {
      await this.getBaseAttachment();
    }

    const wallpapers = [
      ...records.map(record => {
        return {
          ...record,
          ...(record.attachment
            ? {
                wallpaperUrl: `${this.baseAttachmentURL}${record.attachment.location}`,
              }
            : {}),
          category: record.category || "other",
        };
      }),
    ];

    const categories = [
      ...new Set(wallpapers.map(wallpaper => wallpaper.category)),
    ];

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.WALLPAPERS_SET,
        data: wallpapers,
        meta: {
          isStartup,
        },
      })
    );

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.WALLPAPERS_CATEGORY_SET,
        data: categories,
        meta: {
          isStartup,
        },
      })
    );
  }

  initHighlightCounter() {
    let counter = Services.prefs.getIntPref(
      PREF_WALLPAPERS_HIGHLIGHT_SEEN_COUNTER
    );

    this.store.dispatch(
      ac.AlsoToPreloaded({
        type: at.WALLPAPERS_FEATURE_HIGHLIGHT_COUNTER_INCREMENT,
        data: {
          value: counter,
        },
      })
    );
  }

  wallpaperSeenEvent() {
    let counter = Services.prefs.getIntPref(
      PREF_WALLPAPERS_HIGHLIGHT_SEEN_COUNTER
    );

    const newCount = counter + 1;

    this.store.dispatch(
      ac.OnlyToMain({
        type: at.SET_PREF,
        data: {
          name: "newtabWallpapers.highlightSeenCounter",
          value: newCount,
        },
      })
    );

    this.store.dispatch(
      ac.AlsoToPreloaded({
        type: at.WALLPAPERS_FEATURE_HIGHLIGHT_COUNTER_INCREMENT,
        data: {
          value: newCount,
        },
      })
    );
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        await this.wallpaperSetup(true /* isStartup */);
        this.initHighlightCounter();
        break;
      case at.UNINIT:
        break;
      case at.SYSTEM_TICK:
        break;
      case at.PREF_CHANGED:
        if (
          action.data.name === "newtabWallpapers.enabled" ||
          action.data.name === "newtabWallpapers.v2.enabled"
        ) {
          this.wallpaperTeardown();
          await this.wallpaperSetup(false /* isStartup */);
        }
        if (action.data.name === "newtabWallpapers.highlightSeenCounter") {
          // Reset redux highlight counter to pref
          this.initHighlightCounter();
        }
        break;
      case at.WALLPAPERS_SET:
        break;
      case at.WALLPAPERS_FEATURE_HIGHLIGHT_SEEN:
        this.wallpaperSeenEvent();
        break;
    }
  }
}
