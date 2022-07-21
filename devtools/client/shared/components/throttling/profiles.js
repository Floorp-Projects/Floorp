/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const K = 1024;
const M = 1024 * 1024;
const Bps = 1 / 8;
const KBps = K * Bps;
const MBps = M * Bps;

const { LocalizationHelper } = require("devtools/shared/l10n");
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

// Should be synced with devtools/docs/user/network_monitor/throttling/index.rst
const profiles = [
  {
    id: "GPRS",
    download: 50 * KBps,
    upload: 20 * KBps,
    latency: 500,
  },
  {
    id: "Regular 2G",
    download: 250 * KBps,
    upload: 50 * KBps,
    latency: 300,
  },
  {
    id: "Good 2G",
    download: 450 * KBps,
    upload: 150 * KBps,
    latency: 150,
  },
  {
    id: "Regular 3G",
    download: 750 * KBps,
    upload: 250 * KBps,
    latency: 100,
  },
  {
    id: "Good 3G",
    download: 1.5 * MBps,
    upload: 750 * KBps,
    latency: 40,
  },
  {
    id: "Regular 4G / LTE",
    download: 4 * MBps,
    upload: 3 * MBps,
    latency: 20,
  },
  {
    id: "DSL",
    download: 2 * MBps,
    upload: 1 * MBps,
    latency: 5,
  },
  {
    id: "Wi-Fi",
    download: 30 * MBps,
    upload: 15 * MBps,
    latency: 2,
  },
].map(profile => new ThrottlingProfile(profile));

module.exports = profiles;
