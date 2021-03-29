/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";
const RECENTLY_USED_ORDER_DEFAULT = false;
const UI_VERSION = 107;

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

add_task(async function has_not_used_ctrl_tab_and_its_off() {
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  Services.prefs.setBoolPref("browser.engagement.ctrlTab.has-used", false);
  Services.prefs.setBoolPref("browser.ctrlTab.recentlyUsedOrder", false);

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.equal(
    RECENTLY_USED_ORDER_DEFAULT,
    Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed")
  );
});

add_task(async function has_not_used_ctrl_tab_and_its_on() {
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  Services.prefs.setBoolPref("browser.engagement.ctrlTab.has-used", false);
  Services.prefs.setBoolPref("browser.ctrlTab.recentlyUsedOrder", true);

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.equal(
    RECENTLY_USED_ORDER_DEFAULT,
    Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed")
  );
});

add_task(async function has_used_ctrl_tab_and_its_off() {
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  Services.prefs.setBoolPref("browser.engagement.ctrlTab.has-used", true);
  Services.prefs.setBoolPref("browser.ctrlTab.recentlyUsedOrder", false);

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.equal(
    false,
    Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed")
  );
});

add_task(async function has_used_ctrl_tab_and_its_on() {
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  Services.prefs.setBoolPref("browser.engagement.ctrlTab.has-used", true);
  Services.prefs.setBoolPref("browser.ctrlTab.recentlyUsedOrder", true);

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  Assert.equal(
    true,
    Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed")
  );
});

add_task(async function has_used_ctrl_tab_and_its_default() {
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  Services.prefs.setBoolPref("browser.engagement.ctrlTab.has-used", true);
  Services.prefs.clearUserPref("browser.ctrlTab.recentlyUsedOrder");

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  // Default had been true
  Assert.equal(
    true,
    Services.prefs.getBoolPref("browser.ctrlTab.sortByRecentlyUsed")
  );
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.migration.version");
  Services.prefs.clearUserPref("browser.engagement.ctrlTab.has-used");
  Services.prefs.clearUserPref("browser.ctrlTab.recentlyUsedOrder");
  Services.prefs.clearUserPref("browser.ctrlTab.sortByRecentlyUsed");
});
