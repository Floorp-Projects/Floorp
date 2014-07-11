/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["convertToRTCStatsReport"];

function convertToRTCStatsReport(dict) {
  function appendStats(stats, report) {
    stats.forEach(function(stat) {
        report[stat.id] = stat;
      });
  }
  let report = {};
  appendStats(dict.inboundRTPStreamStats, report);
  appendStats(dict.outboundRTPStreamStats, report);
  appendStats(dict.mediaStreamTrackStats, report);
  appendStats(dict.mediaStreamStats, report);
  appendStats(dict.transportStats, report);
  appendStats(dict.iceComponentStats, report);
  appendStats(dict.iceCandidatePairStats, report);
  appendStats(dict.iceCandidateStats, report);
  appendStats(dict.codecStats, report);
  return report;
}

this.convertToRTCStatsReport = convertToRTCStatsReport;
