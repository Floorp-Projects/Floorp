/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

const QPS_PREF = "privacy.query_stripping.enabled";
const STRIP_ON_SHARE_PREF = "privacy.query_stripping.strip_on_share.enabled";

// Tests for the observers for both QPS and Strip on Share
add_setup(async function () {
  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

// Test if Strip on share observers are registered/unregistered depending if the
// Strip on Share Pref is enabled/disabled regardless of the state of QPS Pref
add_task(
  async function checkStripOnShareObserversForVaryingStatesOfQPSAndStripOnShare() {
    for (let queryStrippingEnabled of [false, true]) {
      for (let stripOnShareEnabled of [false, true]) {
        await SpecialPowers.pushPrefEnv({
          set: [
            [QPS_PREF, queryStrippingEnabled],
            [STRIP_ON_SHARE_PREF, stripOnShareEnabled],
          ],
        });

        let areObserservesRegistered;
        await BrowserTestUtils.waitForCondition(function () {
          areObserservesRegistered = listService.testHasStripOnShareObservers();
          return areObserservesRegistered == stripOnShareEnabled;
        }, "waiting for init of URLQueryStrippingListService ensuring observers have time to register if they need");

        if (!stripOnShareEnabled) {
          Assert.ok(!areObserservesRegistered, "Observers are unregistered");
        } else {
          Assert.ok(areObserservesRegistered, "Observers are registered");
        }

        await SpecialPowers.popPrefEnv();
      }
    }
  }
);

// Test if QPS observers are registered/unregistered depending if the QPS
// Pref is enabled/disabled regardless of the state of Strip on Share Pref
add_task(
  async function checkQPSObserversForVaryingStatesOfQPSAndStripOnShare() {
    for (let queryStrippingEnabled of [false, true]) {
      for (let stripOnShareEnabled of [false, true]) {
        await SpecialPowers.pushPrefEnv({
          set: [
            [QPS_PREF, queryStrippingEnabled],
            [STRIP_ON_SHARE_PREF, stripOnShareEnabled],
          ],
        });

        let areObserservesRegistered;
        await BrowserTestUtils.waitForCondition(function () {
          areObserservesRegistered = listService.testHasQPSObservers();
          return areObserservesRegistered == queryStrippingEnabled;
        }, "waiting for init of URLQueryStrippingListService ensuring observers have time to register if they need");

        if (!queryStrippingEnabled) {
          Assert.ok(!areObserservesRegistered, "Observers are unregistered");
        } else {
          Assert.ok(areObserservesRegistered, "Observers are registered");
        }

        await SpecialPowers.popPrefEnv();
      }
    }
  }
);
