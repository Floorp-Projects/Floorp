/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This test may cause intermittents if run at exactly midnight.

const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs"
);
const { AboutProtectionsParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutProtectionsParent.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

ChromeUtils.defineLazyGetter(this, "DB_PATH", function () {
  return PathUtils.join(PathUtils.profileDir, "protections.sqlite");
});

const SQL = {
  insertCustomTimeEvent:
    "INSERT INTO events (type, count, timestamp)" +
    "VALUES (:type, :count, date(:timestamp));",

  selectAll: "SELECT * FROM events",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.database.enabled", true],
      ["browser.vpn_promo.enabled", false],
    ],
  });
});

add_task(async function test_graph_display() {
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  let date = new Date().toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  date = new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 3,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  date = new Date(Date.now() - 2 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 3,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  date = new Date(Date.now() - 3 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 3,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  date = new Date(Date.now() - 4 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  date = new Date(Date.now() - 5 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 3,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 3,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 8,
    timestamp: date,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const DATA_TYPES = [
      "cryptominer",
      "fingerprinter",
      "tracker",
      "cookie",
      "social",
    ];
    let allBars = null;
    await ContentTaskUtils.waitForCondition(() => {
      allBars = content.document.querySelectorAll(".graph-bar");
      return allBars.length;
    }, "The graph has been built");

    Assert.equal(allBars.length, 7, "7 bars have been found on the graph");

    // For accessibility, test if the graph is a table
    // and has a correct column count (number of data types + total + day)
    Assert.equal(
      content.document.getElementById("graph").getAttribute("role"),
      "table",
      "Graph is an accessible table"
    );
    Assert.equal(
      content.document.getElementById("graph").getAttribute("aria-colcount"),
      DATA_TYPES.length + 2,
      "Table has the right number of columns"
    );
    Assert.equal(
      content.document.getElementById("graph").getAttribute("aria-labelledby"),
      "graphLegendDescription",
      "Table has an accessible label"
    );

    // today has each type
    // yesterday will have no tracking cookies
    // 2 days ago will have no fingerprinters
    // 3 days ago will have no cryptominers
    // 4 days ago will have no trackers
    // 5 days ago will have no social (when we add social)
    // 6 days ago will be empty
    Assert.equal(
      allBars[6].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length,
      "today has all of the data types shown"
    );
    Assert.equal(
      allBars[6].getAttribute("role"),
      "row",
      "Today has the correct role"
    );
    Assert.equal(
      allBars[6].getAttribute("aria-owns"),
      "day0 count0 cryptominer0 fingerprinter0 tracker0 cookie0 social0",
      "Row has the columns in the right order"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").style.height,
      "10%",
      "trackers take 10%"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").parentNode.getAttribute("role"),
      "cell",
      "Trackers have the correct role"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").getAttribute("role"),
      "img",
      "Tracker bar has the correct image role"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").getAttribute("aria-label"),
      "1 tracking content (10%)",
      "Trackers have the correct accessible text"
    );
    Assert.equal(
      allBars[6].querySelector(".cryptominer-bar").style.height,
      "20%",
      "cryptominers take 20%"
    );
    Assert.equal(
      allBars[6]
        .querySelector(".cryptominer-bar")
        .parentNode.getAttribute("role"),
      "cell",
      "Cryptominers have the correct role"
    );
    Assert.equal(
      allBars[6].querySelector(".cryptominer-bar").getAttribute("role"),
      "img",
      "Cryptominer bar has the correct image role"
    );
    Assert.equal(
      allBars[6].querySelector(".cryptominer-bar").getAttribute("aria-label"),
      "2 cryptominers (20%)",
      "Cryptominers have the correct accessible label"
    );
    Assert.equal(
      allBars[6].querySelector(".fingerprinter-bar").style.height,
      "20%",
      "fingerprinters take 20%"
    );
    Assert.equal(
      allBars[6]
        .querySelector(".fingerprinter-bar")
        .parentNode.getAttribute("role"),
      "cell",
      "Fingerprinters have the correct role"
    );
    Assert.equal(
      allBars[6].querySelector(".fingerprinter-bar").getAttribute("role"),
      "img",
      "Fingerprinter bar has the correct image role"
    );
    Assert.equal(
      allBars[6].querySelector(".fingerprinter-bar").getAttribute("aria-label"),
      "2 fingerprinters (20%)",
      "Fingerprinters have the correct accessible label"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").style.height,
      "40%",
      "cross site tracking cookies take 40%"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").parentNode.getAttribute("role"),
      "cell",
      "cross site tracking cookies have the correct role"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").getAttribute("role"),
      "img",
      "Cross site tracking cookies bar has the correct image role"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").getAttribute("aria-label"),
      "4 cross-site tracking cookies (40%)",
      "cross site tracking cookies have the correct accessible label"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").style.height,
      "10%",
      "social trackers take 10%"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").parentNode.getAttribute("role"),
      "cell",
      "social trackers have the correct role"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").getAttribute("role"),
      "img",
      "social tracker bar has the correct image role"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").getAttribute("aria-label"),
      "1 social media tracker (10%)",
      "social trackers have the correct accessible text"
    );

    Assert.equal(
      allBars[5].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length - 1,
      "1 day ago is missing one type"
    );
    Assert.ok(
      !allBars[5].querySelector(".cookie-bar"),
      "there is no cross site tracking cookie section 1 day ago."
    );
    Assert.equal(
      allBars[5].getAttribute("aria-owns"),
      "day1 count1 cryptominer1 fingerprinter1 tracker1 social1",
      "Row has the columns in the right order"
    );

    Assert.equal(
      allBars[4].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length - 1,
      "2 days ago is missing one type"
    );
    Assert.ok(
      !allBars[4].querySelector(".fingerprinter-bar"),
      "there is no fingerprinter section 1 day ago."
    );
    Assert.equal(
      allBars[4].getAttribute("aria-owns"),
      "day2 count2 cryptominer2 tracker2 cookie2 social2",
      "Row has the columns in the right order"
    );

    Assert.equal(
      allBars[3].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length - 1,
      "3 days ago is missing one type"
    );
    Assert.ok(
      !allBars[3].querySelector(".cryptominer-bar"),
      "there is no cryptominer section 1 day ago."
    );
    Assert.equal(
      allBars[3].getAttribute("aria-owns"),
      "day3 count3 fingerprinter3 tracker3 cookie3 social3",
      "Row has the columns in the right order"
    );

    Assert.equal(
      allBars[2].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length - 1,
      "4 days ago is missing one type"
    );
    Assert.ok(
      !allBars[2].querySelector(".tracker-bar"),
      "there is no tracker section 1 day ago."
    );
    Assert.equal(
      allBars[2].getAttribute("aria-owns"),
      "day4 count4 cryptominer4 fingerprinter4 cookie4 social4",
      "Row has the columns in the right order"
    );

    Assert.equal(
      allBars[1].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length - 1,
      "5 days ago is missing one type"
    );
    Assert.ok(
      !allBars[1].querySelector(".social-bar"),
      "there is no social section 1 day ago."
    );
    Assert.equal(
      allBars[1].getAttribute("aria-owns"),
      "day5 count5 cryptominer5 fingerprinter5 tracker5 cookie5",
      "Row has the columns in the right order"
    );

    Assert.equal(
      allBars[0].querySelectorAll(".inner-bar").length,
      0,
      "6 days ago has no content"
    );
    Assert.ok(
      allBars[0].classList.contains("empty"),
      "6 days ago is an empty bar"
    );
    Assert.equal(
      allBars[0].getAttribute("aria-owns"),
      "day6 ",
      "Row has the columns in the right order"
    );

    // Check that each tab has the correct aria-labelledby and aria-describedby
    // values. This helps screen readers know what type of tracker the reported
    // tab number is referencing.
    const socialTab = content.document.getElementById("tab-social");
    Assert.equal(
      socialTab.getAttribute("aria-labelledby"),
      "socialLabel socialTitle",
      "aria-labelledby attribute is socialLabel socialTitle"
    );
    Assert.equal(
      socialTab.getAttribute("aria-describedby"),
      "socialContent",
      "aria-describedby attribute is socialContent"
    );

    const cookieTab = content.document.getElementById("tab-cookie");
    Assert.equal(
      cookieTab.getAttribute("aria-labelledby"),
      "cookieLabel cookieTitle",
      "aria-labelledby attribute is cookieLabel cookieTitle"
    );
    Assert.equal(
      cookieTab.getAttribute("aria-describedby"),
      "cookieContent",
      "aria-describedby attribute is cookieContent"
    );

    const trackerTab = content.document.getElementById("tab-tracker");
    Assert.equal(
      trackerTab.getAttribute("aria-labelledby"),
      "trackerLabel trackerTitle",
      "aria-labelledby attribute is trackerLabel trackerTitle"
    );
    Assert.equal(
      trackerTab.getAttribute("aria-describedby"),
      "trackerContent",
      "aria-describedby attribute is trackerContent"
    );

    const fingerprinterTab =
      content.document.getElementById("tab-fingerprinter");
    Assert.equal(
      fingerprinterTab.getAttribute("aria-labelledby"),
      "fingerprinterLabel fingerprinterTitle",
      "aria-labelledby attribute is fingerprinterLabel fingerprinterTitle"
    );
    Assert.equal(
      fingerprinterTab.getAttribute("aria-describedby"),
      "fingerprinterContent",
      "aria-describedby attribute is fingerprinterContent"
    );

    const cryptominerTab = content.document.getElementById("tab-cryptominer");
    Assert.equal(
      cryptominerTab.getAttribute("aria-labelledby"),
      "cryptominerLabel cryptominerTitle",
      "aria-labelledby attribute is cryptominerLabel cryptominerTitle"
    );
    Assert.equal(
      cryptominerTab.getAttribute("aria-describedby"),
      "cryptominerContent",
      "aria-describedby attribute is cryptominerContent"
    );
  });

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  let rows = await db.execute(SQL.selectAll);
  is(rows.length, 0, "length is 0");
  await db.close();
  BrowserTestUtils.removeTab(tab);
});

// Ensure that the number of suspicious fingerprinter is aggregated into the
// fingerprinter category on about:protection page.
add_task(async function test_suspicious_fingerprinter() {
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  // Inserting data for today. It won't contain a fingerprinter entry but only
  // a suspicious fingerprinter entry.
  let date = new Date().toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SUSPICIOUS_FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  // Inserting data for 1 day age. It contains both a fingerprinter entry and
  // a suspicious fingerprinter entry.
  date = new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SUSPICIOUS_FINGERPRINTERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const DATA_TYPES = [
      "cryptominer",
      "fingerprinter",
      "tracker",
      "cookie",
      "social",
    ];
    let allBars = null;
    await ContentTaskUtils.waitForMutationCondition(
      content.document.body,
      { childList: true, subtree: true },
      () => {
        allBars = content.document.querySelectorAll(".graph-bar");
        return !!allBars.length;
      }
    );
    info("The graph has been built");

    Assert.equal(allBars.length, 7, "7 bars have been found on the graph");

    // Verify today's data. The fingerprinter category should take 20%.
    Assert.equal(
      allBars[6].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length,
      "today has all of the data types shown"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").style.height,
      "10%",
      "trackers take 10%"
    );
    Assert.equal(
      allBars[6].querySelector(".cryptominer-bar").style.height,
      "20%",
      "cryptominers take 20%"
    );
    Assert.equal(
      allBars[6].querySelector(".fingerprinter-bar").style.height,
      "20%",
      "fingerprinters take 20%"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").style.height,
      "40%",
      "cross site tracking cookies take 40%"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").style.height,
      "10%",
      "social trackers take 10%"
    );

    // Verify one day age data. The fingerprinter category should take 20%.
    Assert.equal(
      allBars[5].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length,
      "today has all of the data types shown"
    );
    Assert.equal(
      allBars[5].querySelector(".tracker-bar").style.height,
      "10%",
      "trackers take 10%"
    );
    Assert.equal(
      allBars[5].querySelector(".cryptominer-bar").style.height,
      "20%",
      "cryptominers take 20%"
    );
    Assert.equal(
      allBars[5].querySelector(".fingerprinter-bar").style.height,
      "20%",
      "fingerprinters take 20%"
    );
    Assert.equal(
      allBars[5].querySelector(".cookie-bar").style.height,
      "40%",
      "cross site tracking cookies take 40%"
    );
    Assert.equal(
      allBars[5].querySelector(".social-bar").style.height,
      "10%",
      "social trackers take 10%"
    );
  });

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  let rows = await db.execute(SQL.selectAll);
  is(rows.length, 0, "length is 0");
  await db.close();
  BrowserTestUtils.removeTab(tab);
});

// Ensure that the number of suspicious fingerprinter is displayed even if the
// fingerprinter blocking is disabled.
add_task(async function test_suspicious_fingerprinter_without_fp_blocking() {
  // Disable fingerprinter blocking
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.fingerprinting.enabled",
    false
  );

  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  // Inserting data for today. It won't contain a fingerprinter entry but only
  // a suspicious fingerprinter entry.
  let date = new Date().toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 1,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SUSPICIOUS_FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 4,
    timestamp: date,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.SOCIAL_ID,
    count: 1,
    timestamp: date,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const DATA_TYPES = [
      "cryptominer",
      "fingerprinter",
      "tracker",
      "cookie",
      "social",
    ];
    let allBars = null;
    await ContentTaskUtils.waitForMutationCondition(
      content.document.body,
      { childList: true, subtree: true },
      () => {
        allBars = content.document.querySelectorAll(".graph-bar");
        return !!allBars.length;
      }
    );
    info("The graph has been built");

    Assert.equal(allBars.length, 7, "7 bars have been found on the graph");

    // Verify today's data. The fingerprinter category should take 20%.
    Assert.equal(
      allBars[6].querySelectorAll(".inner-bar").length,
      DATA_TYPES.length,
      "today has all of the data types shown"
    );
    Assert.equal(
      allBars[6].querySelector(".tracker-bar").style.height,
      "10%",
      "trackers take 10%"
    );
    Assert.equal(
      allBars[6].querySelector(".cryptominer-bar").style.height,
      "20%",
      "cryptominers take 20%"
    );
    Assert.equal(
      allBars[6].querySelector(".fingerprinter-bar").style.height,
      "20%",
      "fingerprinters take 20%"
    );
    Assert.equal(
      allBars[6].querySelector(".cookie-bar").style.height,
      "40%",
      "cross site tracking cookies take 40%"
    );
    Assert.equal(
      allBars[6].querySelector(".social-bar").style.height,
      "10%",
      "social trackers take 10%"
    );
  });

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  let rows = await db.execute(SQL.selectAll);
  is(rows.length, 0, "length is 0");
  await db.close();
  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(
    "privacy.trackingprotection.fingerprinting.enabled"
  );
});

// Ensure that each type of tracker is hidden from the graph if there are no recorded
// trackers of that type and the user has chosen to not block that type.
add_task(async function test_etp_custom_settings() {
  Services.prefs.setStringPref("browser.contentblocking.category", "strict");
  Services.prefs.setBoolPref(
    "privacy.socialtracking.block_cookies.enabled",
    true
  );
  // hide cookies from the graph
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let legend = content.document.getElementById("legend");
      return ContentTaskUtils.isVisible(legend);
    }, "The legend is visible");

    let label = content.document.getElementById("cookieLabel");
    Assert.ok(ContentTaskUtils.isHidden(label), "Cookie Label is hidden");

    label = content.document.getElementById("trackerLabel");
    Assert.ok(ContentTaskUtils.isVisible(label), "Tracker Label is visible");
    label = content.document.getElementById("socialLabel");
    Assert.ok(ContentTaskUtils.isVisible(label), "Social Label is visible");
    label = content.document.getElementById("cryptominerLabel");
    Assert.ok(
      ContentTaskUtils.isVisible(label),
      "Cryptominer Label is visible"
    );
    label = content.document.getElementById("fingerprinterLabel");
    Assert.ok(
      ContentTaskUtils.isVisible(label),
      "Fingerprinter Label is visible"
    );
  });
  BrowserTestUtils.removeTab(tab);

  // hide ad trackers from the graph
  Services.prefs.setBoolPref("privacy.trackingprotection.enabled", false);
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let legend = content.document.getElementById("legend");
      return ContentTaskUtils.isVisible(legend);
    }, "The legend is visible");

    let label = content.document.querySelector("#trackerLabel");
    Assert.ok(ContentTaskUtils.isHidden(label), "Tracker Label is hidden");

    label = content.document.querySelector("#socialLabel");
    Assert.ok(ContentTaskUtils.isHidden(label), "Social Label is hidden");
  });
  BrowserTestUtils.removeTab(tab);

  // hide social from the graph
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.socialtracking.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "privacy.socialtracking.block_cookies.enabled",
    false
  );
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let legend = content.document.getElementById("legend");
      return ContentTaskUtils.isVisible(legend);
    }, "The legend is visible");

    let label = content.document.querySelector("#socialLabel");
    Assert.ok(ContentTaskUtils.isHidden(label), "Social Label is hidden");
  });
  BrowserTestUtils.removeTab(tab);

  // hide fingerprinting from the graph
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.fingerprinting.enabled",
    false
  );
  Services.prefs.setBoolPref("privacy.fingerprintingProtection", false);
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let legend = content.document.getElementById("legend");
      return ContentTaskUtils.isVisible(legend);
    }, "The legend is visible");

    let label = content.document.querySelector("#fingerprinterLabel");
    Assert.ok(
      ContentTaskUtils.isHidden(label),
      "Fingerprinter Label is hidden"
    );
  });
  BrowserTestUtils.removeTab(tab);

  // hide cryptomining from the graph
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.cryptomining.enabled",
    false
  );
  // Turn fingerprinting on so that all protectionsare not turned off, otherwise we will get a special card.
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.fingerprinting.enabled",
    true
  );
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let legend = content.document.getElementById("legend");
      return ContentTaskUtils.isVisible(legend);
    }, "The legend is visible");

    let label = content.document.querySelector("#cryptominerLabel");
    Assert.ok(ContentTaskUtils.isHidden(label), "Cryptominer Label is hidden");
  });
  Services.prefs.clearUserPref("browser.contentblocking.category");
  Services.prefs.clearUserPref(
    "privacy.trackingprotection.fingerprinting.enabled"
  );
  Services.prefs.clearUserPref("privacy.fingerprintingProtection");
  Services.prefs.clearUserPref(
    "privacy.trackingprotection.cryptomining.enabled"
  );
  Services.prefs.clearUserPref("privacy.trackingprotection.enabled");
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref("privacy.socialtracking.block_cookies.enabled");

  BrowserTestUtils.removeTab(tab);
});

// Ensure that the Custom manage Protections card is shown if the user has all protections turned off.
add_task(async function test_etp_custom_protections_off() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.category", "custom"],
      ["network.cookie.cookieBehavior", 0], // not blocking
      ["privacy.trackingprotection.cryptomining.enabled", false], // not blocking
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.socialtracking.enabled", false],
      ["privacy.socialtracking.block_cookies.enabled", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  let aboutPreferencesPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:preferences#privacy"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let etpCard = content.document.querySelector(".etp-card");
      return etpCard.classList.contains("custom-not-blocking");
    }, "The custom protections warning card is showing");

    let manageProtectionsButton =
      content.document.getElementById("manage-protections");
    Assert.ok(
      ContentTaskUtils.isVisible(manageProtectionsButton),
      "Button to manage protections is displayed"
    );
  });

  // Custom protection card should show, even if there would otherwise be data on the graph.
  let db = await Sqlite.openConnection({ path: DB_PATH });
  let date = new Date().toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 1,
    timestamp: date,
  });
  await BrowserTestUtils.reloadTab(tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      let etpCard = content.document.querySelector(".etp-card");
      return etpCard.classList.contains("custom-not-blocking");
    }, "The custom protections warning card is showing");

    let manageProtectionsButton =
      content.document.getElementById("manage-protections");
    Assert.ok(
      ContentTaskUtils.isVisible(manageProtectionsButton),
      "Button to manage protections is displayed"
    );

    manageProtectionsButton.click();
  });
  let aboutPreferencesTab = await aboutPreferencesPromise;
  info("about:preferences#privacy was successfully opened in a new tab");
  gBrowser.removeTab(aboutPreferencesTab);

  Services.prefs.setStringPref("browser.contentblocking.category", "standard");
  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  let rows = await db.execute(SQL.selectAll);
  is(rows.length, 0, "length is 0");
  await db.close();
  BrowserTestUtils.removeTab(tab);
});

// Ensure that the ETP mobile promotion card is shown when the pref is on and
// there are no mobile devices connected.
add_task(async function test_etp_mobile_promotion_pref_on() {
  AboutProtectionsParent.setTestOverride(mockGetLoginDataWithSyncedDevices());
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.show_mobile_app", true]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let mobilePromotion = content.document.getElementById("mobile-hanger");
    Assert.ok(
      ContentTaskUtils.isVisible(mobilePromotion),
      "Mobile promotions card is displayed when pref is on and there are no synced mobile devices"
    );

    // Card should hide after the X is clicked.
    mobilePromotion.querySelector(".exit-icon").click();
    Assert.ok(
      ContentTaskUtils.isHidden(mobilePromotion),
      "Mobile promotions card is no longer displayed after clicking the X button"
    );
  });
  BrowserTestUtils.removeTab(tab);

  // Add a mock mobile device. The promotion should now be hidden.
  AboutProtectionsParent.setTestOverride(
    mockGetLoginDataWithSyncedDevices(true)
  );
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let mobilePromotion = content.document.getElementById("mobile-hanger");
    Assert.ok(
      ContentTaskUtils.isHidden(mobilePromotion),
      "Mobile promotions card is hidden when pref is on if there are synced mobile devices"
    );
  });

  BrowserTestUtils.removeTab(tab);
  AboutProtectionsParent.setTestOverride(null);
});

// Test that ETP mobile promotion is not shown when the pref is off,
// even if no mobile devices are synced.
add_task(async function test_etp_mobile_promotion_pref_on() {
  AboutProtectionsParent.setTestOverride(mockGetLoginDataWithSyncedDevices());
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.show_mobile_app", false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let mobilePromotion = content.document.getElementById("mobile-hanger");
    Assert.ok(
      ContentTaskUtils.isHidden(mobilePromotion),
      "Mobile promotions card is not displayed when pref is off and there are no synced mobile devices"
    );
  });

  BrowserTestUtils.removeTab(tab);

  AboutProtectionsParent.setTestOverride(
    mockGetLoginDataWithSyncedDevices(true)
  );
  tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let mobilePromotion = content.document.getElementById("mobile-hanger");
    Assert.ok(
      ContentTaskUtils.isHidden(mobilePromotion),
      "Mobile promotions card is not displayed when pref is off even if there are synced mobile devices"
    );
  });
  BrowserTestUtils.removeTab(tab);
  AboutProtectionsParent.setTestOverride(null);
});

// Test that clicking on the link to settings in the header properly opens the settings page.
add_task(async function test_settings_links() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  let aboutPreferencesPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:preferences#privacy"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const protectionSettings = await ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("protection-settings");
    }, "protection-settings link exists");

    protectionSettings.click();
  });
  let aboutPreferencesTab = await aboutPreferencesPromise;
  info("about:preferences#privacy was successfully opened in a new tab");
  gBrowser.removeTab(aboutPreferencesTab);
  gBrowser.removeTab(tab);
});
