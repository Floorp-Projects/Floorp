/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if "DNS Resolution" is displayed in the headers panel and
 * that requests that are resolve over DNS over HTTPS are accuratelly tracked.
 * Note: Test has to run as a http3 test for the DoH servers to be used
 */

add_task(async function testCheckDNSResolution() {
  Services.dns.clearCache(true);
  // Force to use DNS Trusted Recursive Resolver only
  await pushPref("network.trr.mode", 3);

  const { monitor } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    enableCache: true,
    requestCount: 1,
  });

  const { document } = monitor.panelWin;

  const requestsComplete = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await requestsComplete;

  // Wait until the tab panel summary is displayed
  const waitForTab = waitUntil(
    () => document.querySelectorAll(".tabpanel-summary-label")[0]
  );
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await waitForTab;

  const dnsEl = [...document.querySelectorAll(".headers-summary")].find(
    el =>
      el.querySelector(".headers-summary-label").textContent ===
      "DNS Resolution"
  );

  is(
    dnsEl.querySelector(".tabpanel-summary-value").textContent,
    L10N.getStr(`netmonitor.headers.dns.overHttps`),
    "The DNS Resolution value showed correctly"
  );

  return teardown(monitor);
});
