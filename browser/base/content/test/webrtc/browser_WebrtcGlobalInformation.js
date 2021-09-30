/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
});

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
  };

  reports.forEach(sanityCheckReport);
  return reports;
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
