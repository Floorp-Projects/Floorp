/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
  Ci.nsIProcessToolsService
);

let getStatsReports = async (filter = "") => {
  let { reports } = await new Promise(r =>
    WebrtcGlobalInformation.getAllStats(r, filter)
  );

  ok(Array.isArray(reports), "|reports| is an array");

  let sanityCheckReport = report => {
    isnot(report.pcid, "", "pcid is non-empty");
    if (filter.length) {
      is(report.pcid, filter, "pcid matches filter");
    }

    // Check for duplicates
    const checkForDuplicateId = statsArray => {
      ok(Array.isArray(statsArray), "|statsArray| is an array");
      const ids = new Set();
      statsArray.forEach(stat => {
        is(typeof stat.id, "string", "|stat.id| is a string");
        ok(
          !ids.has(stat.id),
          `Id ${stat.id} should appear only once. Stat was ${JSON.stringify(
            stat
          )}`
        );
        ids.add(stat.id);
      });
    };

    checkForDuplicateId(report.inboundRtpStreamStats);
    checkForDuplicateId(report.outboundRtpStreamStats);
    checkForDuplicateId(report.remoteInboundRtpStreamStats);
    checkForDuplicateId(report.remoteOutboundRtpStreamStats);
    checkForDuplicateId(report.rtpContributingSourceStats);
    checkForDuplicateId(report.iceCandidatePairStats);
    checkForDuplicateId(report.iceCandidateStats);
    checkForDuplicateId(report.trickledIceCandidateStats);
    checkForDuplicateId(report.dataChannelStats);
    checkForDuplicateId(report.codecStats);
  };

  reports.forEach(sanityCheckReport);
  return reports;
};

const getStatsHistoryPcIds = async () => {
  return new Promise(r => WebrtcGlobalInformation.getStatsHistoryPcIds(r));
};

const getStatsHistorySince = async (pcid, after, sdpAfter) => {
  return new Promise(r =>
    WebrtcGlobalInformation.getStatsHistorySince(r, pcid, after, sdpAfter)
  );
};

let getLogging = async () => {
  let logs = await new Promise(r => WebrtcGlobalInformation.getLogging("", r));
  ok(Array.isArray(logs), "|logs| is an array");
  return logs;
};

let checkStatsReportCount = async (count, filter = "") => {
  let reports = await getStatsReports(filter);
  is(reports.length, count, `|reports| should have length ${count}`);
  if (reports.length != count) {
    info(`reports = ${JSON.stringify(reports)}`);
  }
  return reports;
};

let checkLoggingEmpty = async () => {
  let logs = await getLogging();
  is(logs.length, 0, "Logging is empty");
  if (logs.length) {
    info(`logs = ${JSON.stringify(logs)}`);
  }
  return logs;
};

let checkLoggingNonEmpty = async () => {
  let logs = await getLogging();
  isnot(logs.length, 0, "Logging is not empty");
  return logs;
};

let clearAndCheck = async () => {
  WebrtcGlobalInformation.clearAllStats();
  WebrtcGlobalInformation.clearLogging();
  await checkStatsReportCount(0);
  await checkLoggingEmpty();
};

let openTabInNewProcess = async file => {
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  );
  let absoluteURI = rootDir + file;

  return BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: absoluteURI,
    forceNewProcess: true,
  });
};

let killTabProcess = async tab => {
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    ChromeUtils.privateNoteIntentionalCrash();
  });
  ProcessTools.kill(tab.linkedBrowser.frameLoader.remoteTab.osPid);
};

add_task(async () => {
  info("Test that clearAllStats is callable");
  WebrtcGlobalInformation.clearAllStats();
  ok(true, "clearAllStats returns");
});

add_task(async () => {
  info("Test that clearLogging is callable");
  WebrtcGlobalInformation.clearLogging();
  ok(true, "clearLogging returns");
});

add_task(async () => {
  info(
    "Test that getAllStats is callable, and returns 0 results when no RTCPeerConnections have existed"
  );
  await checkStatsReportCount(0);
});

add_task(async () => {
  info(
    "Test that getLogging is callable, and returns 0 results when no RTCPeerConnections have existed"
  );
  await checkLoggingEmpty();
});

add_task(async () => {
  info("Test that we can get stats/logging for a PC on the parent process");
  await clearAndCheck();
  let pc = new RTCPeerConnection();
  await pc.setLocalDescription(
    await pc.createOffer({ offerToReceiveAudio: true })
  );
  // Let ICE stack go quiescent
  await new Promise(r => {
    pc.onicegatheringstatechange = () => {
      if (pc.iceGatheringState == "complete") {
        r();
      }
    };
  });
  await checkStatsReportCount(1);
  await checkLoggingNonEmpty();
  pc.close();
  pc = null;
  // Closing a PC should not do anything to the ICE logging
  await checkLoggingNonEmpty();
  // There's just no way to get a signal that the ICE stack has stopped logging
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2000));
  await clearAndCheck();
});

add_task(async () => {
  info("Test that we can get stats/logging for a PC on a content process");
  await clearAndCheck();
  let tab = await openTabInNewProcess("single_peerconnection.html");
  await checkStatsReportCount(1);
  await checkLoggingNonEmpty();
  await killTabProcess(tab);
  BrowserTestUtils.removeTab(tab);
  await clearAndCheck();
});

add_task(async () => {
  info(
    "Test that we can get stats/logging for two connected PCs on a content process"
  );
  await clearAndCheck();
  let tab = await openTabInNewProcess("peerconnection_connect.html");
  await checkStatsReportCount(2);
  await checkLoggingNonEmpty();
  await killTabProcess(tab);
  BrowserTestUtils.removeTab(tab);
  await clearAndCheck();
});

add_task(async () => {
  info("Test filtering for stats reports (parent process)");
  await clearAndCheck();
  let pc1 = new RTCPeerConnection();
  let pc2 = new RTCPeerConnection();
  let allReports = await checkStatsReportCount(2);
  await checkStatsReportCount(1, allReports[0].pcid);
  pc1.close();
  pc2.close();
  pc1 = null;
  pc2 = null;
  await checkStatsReportCount(1, allReports[0].pcid);
  await clearAndCheck();
});

add_task(async () => {
  info("Test filtering for stats reports (content process)");
  await clearAndCheck();
  let tab1 = await openTabInNewProcess("single_peerconnection.html");
  let tab2 = await openTabInNewProcess("single_peerconnection.html");
  let allReports = await checkStatsReportCount(2);
  await checkStatsReportCount(1, allReports[0].pcid);
  await killTabProcess(tab1);
  BrowserTestUtils.removeTab(tab1);
  await killTabProcess(tab2);
  BrowserTestUtils.removeTab(tab2);
  await checkStatsReportCount(1, allReports[0].pcid);
  await clearAndCheck();
});

add_task(async () => {
  info("Test that stats/logging persists when PC is closed (parent process)");
  await clearAndCheck();
  let pc = new RTCPeerConnection();
  // This stuff will generate logging
  await pc.setLocalDescription(
    await pc.createOffer({ offerToReceiveAudio: true })
  );
  // Once gathering is done, the ICE stack should go quiescent
  await new Promise(r => {
    pc.onicegatheringstatechange = () => {
      if (pc.iceGatheringState == "complete") {
        r();
      }
    };
  });
  let reports = await checkStatsReportCount(1);
  isnot(
    window.browsingContext.browserId,
    undefined,
    "browserId is defined for parent process"
  );
  is(
    reports[0].browserId,
    window.browsingContext.browserId,
    "browserId for stats report matches parent process"
  );
  await checkLoggingNonEmpty();
  pc.close();
  pc = null;
  await checkStatsReportCount(1);
  await checkLoggingNonEmpty();
  await clearAndCheck();
});

add_task(async () => {
  info("Test that stats/logging persists when PC is closed (content process)");
  await clearAndCheck();
  let tab = await openTabInNewProcess("single_peerconnection.html");
  let { browserId } = tab.linkedBrowser;
  let reports = await checkStatsReportCount(1);
  is(reports[0].browserId, browserId, "browserId for stats report matches tab");
  isnot(
    browserId,
    window.browsingContext.browserId,
    "tab browser id is not the same as parent process browser id"
  );
  await checkLoggingNonEmpty();
  await killTabProcess(tab);
  BrowserTestUtils.removeTab(tab);
  await checkStatsReportCount(1);
  await checkLoggingNonEmpty();
  await clearAndCheck();
});

const set_int_pref_returning_unsetter = (pref, num) => {
  const value = Services.prefs.getIntPref(pref);
  Services.prefs.setIntPref(pref, num);
  return () => Services.prefs.setIntPref(pref, value);
};

const stats_history_is_enabled = () => {
  return Services.prefs.getBoolPref("media.aboutwebrtc.hist.enabled");
};

const set_max_histories_to_retain = num =>
  set_int_pref_returning_unsetter(
    "media.aboutwebrtc.hist.closed_stats_to_retain",
    num
  );

const set_history_storage_window_s = num =>
  set_int_pref_returning_unsetter(
    "media.aboutwebrtc.hist.storage_window_s",
    num
  );

add_task(async () => {
  if (!stats_history_is_enabled()) {
    return;
  }
  info(
    "Test that stats history is available after close until clearLongTermStats is called"
  );
  await clearAndCheck();
  const pc = new RTCPeerConnection();

  const ids = await getStatsHistoryPcIds();
  is(ids.length, 1, "There is a single PeerConnection Id for stats history.");

  let firstLen = 0;
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2000));
  {
    const history = await getStatsHistorySince(ids[0]);
    firstLen = history.reports.length;
    ok(
      history.reports.length,
      "There is at least a single PeerConnection stats history before close."
    );
  }
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2000));
  {
    const history = await getStatsHistorySince(ids[0]);
    const secondLen = history.reports.length;
    Assert.greater(
      secondLen,
      firstLen,
      "After waiting there are more history entries available."
    );
  }
  pc.close();
  // After close for final stats and pc teardown to settle
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2000));
  {
    const history = await getStatsHistorySince(ids[0]);
    ok(
      history.reports.length,
      "There is at least a single PeerConnection stats history after close."
    );
  }
  await clearAndCheck();
  {
    const history = await getStatsHistorySince(ids[0]);
    is(
      history.reports.length,
      0,
      "After PC.close and clearing the stats there are no history reports"
    );
  }
  {
    const ids1 = await getStatsHistoryPcIds();
    is(
      ids1.length,
      0,
      "After PC.close and clearing the stats there are no history pcids"
    );
  }
  {
    const pc2 = new RTCPeerConnection();
    const pc3 = new RTCPeerConnection();
    let idsN = await getStatsHistoryPcIds();
    is(
      idsN.length,
      2,
      "There are two pcIds after creating two PeerConnections"
    );
    pc2.close();
    // After close for final stats and pc teardown to settle
    // I "don't love" this but we don't have a anything we can await on ... yet.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 2000));
    await WebrtcGlobalInformation.clearAllStats();
    idsN = await getStatsHistoryPcIds();
    is(
      idsN.length,
      1,
      "There is one pcIds after closing one of two PeerConnections and clearing stats"
    );
    pc3.close();
    // After close for final stats and pc teardown to settle
    // I "don't love" this but we don't have a anything we can await on ... yet.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 2000));
  }
});

add_task(async () => {
  if (!stats_history_is_enabled()) {
    return;
  }
  const restoreHistRetainPref = set_max_histories_to_retain(7);
  info("Test that the proper number of pcIds are available");
  await clearAndCheck();
  const pc01 = new RTCPeerConnection();
  const pc02 = new RTCPeerConnection();
  const pc03 = new RTCPeerConnection();
  const pc04 = new RTCPeerConnection();
  const pc05 = new RTCPeerConnection();
  const pc06 = new RTCPeerConnection();
  const pc07 = new RTCPeerConnection();
  const pc08 = new RTCPeerConnection();
  const pc09 = new RTCPeerConnection();
  const pc10 = new RTCPeerConnection();
  const pc11 = new RTCPeerConnection();
  const pc12 = new RTCPeerConnection();
  const pc13 = new RTCPeerConnection();
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2000));
  {
    const ids = await getStatsHistoryPcIds();
    is(ids.length, 13, "There is are 13 PeerConnection Ids for stats history.");
  }
  pc01.close();
  pc02.close();
  pc03.close();
  pc04.close();
  pc05.close();
  pc06.close();
  pc07.close();
  pc08.close();
  pc09.close();
  pc10.close();
  pc11.close();
  pc12.close();
  pc13.close();
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 5000));
  {
    const ids = await getStatsHistoryPcIds();
    is(
      ids.length,
      7,
      "After closing 13 PCs there are no more than the max closed (7) PeerConnection Ids for stats history."
    );
  }
  restoreHistRetainPref();
  await clearAndCheck();
});

add_task(async () => {
  if (!stats_history_is_enabled()) {
    return;
  }
  // If you change this, please check if the setTimeout should be updated.
  // NOTE: the unit here is _integer_ seconds.
  const STORAGE_WINDOW_S = 1;
  const restoreStorageWindowPref =
    set_history_storage_window_s(STORAGE_WINDOW_S);
  info("Test that history items are being aged out");
  await clearAndCheck();
  const pc = new RTCPeerConnection();
  // I "don't love" this but we don't have a anything we can await on ... yet.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, STORAGE_WINDOW_S * 2 * 1000));
  const ids = await getStatsHistoryPcIds();
  const { reports } = await getStatsHistorySince(ids[0]);
  const first = reports[0];
  const last = reports.at(-1);
  Assert.lessOrEqual(
    last.timestamp - first.timestamp,
    STORAGE_WINDOW_S * 1000,
    "History reports should be aging out according to the storage window pref"
  );
  pc.close();
  restoreStorageWindowPref();
  await clearAndCheck();
});
