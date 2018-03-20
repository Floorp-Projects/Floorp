/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL = "http://example.com/browser/dom/tests/browser/dummy.html";
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");


add_task(async function test() {
  if (!AppConstants.RELEASE_OR_BETA) {
    SpecialPowers.setBoolPref('dom.performance.enable_scheduler_timing', true);
    waitForExplicitFinish();

    await BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com" },
      async function(browser) {

      // grab events..
      var events = [];
      function getInfoFromService(subject, topic, value) {
        subject = subject.QueryInterface(Ci.nsIPerformanceMetricsData);
        if (subject.host == "example.com") {
          events.push(subject);
        }
      }
      Services.obs.addObserver(getInfoFromService, "performance-metrics");

      // trigger an IPDL call
      ChromeUtils.requestPerformanceMetrics();

      // wait until we get the events back
      await BrowserTestUtils.waitForCondition(() => {
        return events.length > 0;
      }, "wait for events to come in", 100, 20);

      // let's check the last example.com tab event we got
      let last = events[0];
      Assert.equal(last.host, "example.com", "host should be example.com");
      Assert.ok(last.duration > 0, "Duration should be positive");

      // let's look at the XPCOM data we got back
      let items = last.items.QueryInterface(Ci.nsIMutableArray);
      let enumerator = items.enumerate();
      let total = 0;
      while (enumerator.hasMoreElements()) {
        let item = enumerator.getNext();
        item = item.QueryInterface(Ci.nsIPerformanceMetricsDispatchCategory);
        total += item.count;
      }
      Assert.ok(total > 0);
    });
    SpecialPowers.clearUserPref('dom.performance.enable_scheduler_timing');
  }
});
