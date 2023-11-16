/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const K = 1024;
const M = 1024 * 1024;
const Bps = 1 / 8;
const KBps = K * Bps;
const MBps = M * Bps;

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/network-throttling.properties"
);

/**
 * Predefined network throttling profiles.
 * Speeds are in bytes per second.  Latency is in ms.
 */

class ThrottlingProfile {
  constructor({ id, download, upload, latency }) {
    this.id = id;
    this.download = download;
    this.upload = upload;
    this.latency = latency;
  }

  get description() {
    const download = this.#toDescriptionData(this.download);
    const upload = this.#toDescriptionData(this.upload);
    return L10N.getFormatStr(
      "throttling.profile.description",
      download.value,
      download.unit,
      upload.value,
      upload.unit,
      this.latency
    );
  }

  #toDescriptionData(val) {
    if (val % MBps === 0) {
      return { value: val / MBps, unit: "Mbps" };
    }
    return { value: val / KBps, unit: "Kbps" };
  }
}

const PROFILE_CONSTANTS = {
  GPRS: "GPRS",
  REGULAR_2G: "Regular 2G",
  GOOD_2G: "Good 2G",
  REGULAR_3G: "Regular 3G",
  GOOD_3G: "Good 3G",
  REGULAR_4G_LTE: "Regular 4G / LTE",
  DSL: "DSL",
  WIFI: "Wi-Fi",
  OFFLINE: "Offline",
};

// Should be synced with devtools/docs/user/network_monitor/throttling/index.rst
const profiles = [
  {
    id: PROFILE_CONSTANTS.GPRS,
    download: 50 * KBps,
    upload: 20 * KBps,
    latency: 500,
  },
  {
    id: PROFILE_CONSTANTS.REGULAR_2G,
    download: 250 * KBps,
    upload: 50 * KBps,
    latency: 300,
  },
  {
    id: PROFILE_CONSTANTS.GOOD_2G,
    download: 450 * KBps,
    upload: 150 * KBps,
    latency: 150,
  },
  {
    id: PROFILE_CONSTANTS.REGULAR_3G,
    download: 750 * KBps,
    upload: 250 * KBps,
    latency: 100,
  },
  {
    id: PROFILE_CONSTANTS.GOOD_3G,
    download: 1.5 * MBps,
    upload: 750 * KBps,
    latency: 40,
  },
  {
    id: PROFILE_CONSTANTS.REGULAR_4G_LTE,
    download: 4 * MBps,
    upload: 3 * MBps,
    latency: 20,
  },
  {
    id: PROFILE_CONSTANTS.DSL,
    download: 2 * MBps,
    upload: 1 * MBps,
    latency: 5,
  },
  {
    id: PROFILE_CONSTANTS.WIFI,
    download: 30 * MBps,
    upload: 15 * MBps,
    latency: 2,
  },
  {
    id: PROFILE_CONSTANTS.OFFLINE,
    download: 0,
    upload: 0,
    latency: 5,
  },
].map(profile => new ThrottlingProfile(profile));

module.exports = { profiles, PROFILE_CONSTANTS };
