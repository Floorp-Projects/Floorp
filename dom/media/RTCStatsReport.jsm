/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["convertToRTCStatsReport"];

function convertToRTCStatsReport(dict) {
  function appendStats(stats, report) {
    stats.forEach(function(stat) {
        report[stat.id] = stat;
      });
  }
  let report = {};
  appendStats(dict.inboundRtpStreamStats, report);
  appendStats(dict.outboundRtpStreamStats, report);
  appendStats(dict.remoteInboundRtpStreamStats, report);
  appendStats(dict.remoteOutboundRtpStreamStats, report);
  appendStats(dict.rtpContributingSourceStats, report);
  appendStats(dict.iceCandidatePairStats, report);
  appendStats(dict.iceCandidateStats, report);
  return report;
}

