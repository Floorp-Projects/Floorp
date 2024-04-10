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

export class WallpaperFeed {
  constructor() {
    this.loaded = false;
    this.wallpaperClient = "";
    this.wallpaperDB = "";
    this.baseAttachmentURL = "";
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

  async wallpaperSetup() {
    const wallpapersEnabled = Services.prefs.getBoolPref(
      PREF_WALLPAPERS_ENABLED
    );

    if (wallpapersEnabled) {
      if (!this.wallpaperClient) {
        this.wallpaperClient = this.RemoteSettings("newtab-wallpapers");
      }

      await this.getBaseAttachment();
      this.wallpaperClient.on("sync", () => this.updateWallpapers());
      this.updateWallpapers();
    }
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

  async updateWallpapers() {
    const records = await this.wallpaperClient.get();
    if (!records?.length) {
      return;
    }

    if (!this.baseAttachmentURL) {
      await this.getBaseAttachment();
    }
    const wallpapers = records.map(record => {
      return {
        ...record,
        wallpaperUrl: `${this.baseAttachmentURL}${record.attachment.location}`,
      };
    });

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.WALLPAPERS_SET,
        data: wallpapers,
      })
    );
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        await this.wallpaperSetup();
        break;
      case at.UNINIT:
        break;
      case at.SYSTEM_TICK:
        break;
      case at.PREF_CHANGED:
        if (action.data.name === "newtabWallpapers.enabled") {
          await this.wallpaperSetup();
        }
        break;
      case at.WALLPAPERS_SET:
        break;
    }
  }
}
