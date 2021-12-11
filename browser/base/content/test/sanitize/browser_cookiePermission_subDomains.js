const { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

// 2 domains: www.mozilla.org (session-only) mozilla.org (allowed) - after the
// cleanp, mozilla.org must have data.
add_task(async function subDomains1() {
  info("Test subdomains and custom setting");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_NORMALLY],
      ["browser.sanitizer.loglevel", "All"],
    ],
  });

  // Domains and data
  let originA = "https://www.mozilla.org";
  PermissionTestUtils.add(
    originA,
    "cookie",
    Ci.nsICookiePermission.ACCESS_SESSION
  );

  SiteDataTestUtils.addToCookies({ origin: originA });
  await SiteDataTestUtils.addToIndexedDB(originA);

  let originB = "https://mozilla.org";
  PermissionTestUtils.add(
    originB,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  SiteDataTestUtils.addToCookies({ origin: originB });
  await SiteDataTestUtils.addToIndexedDB(originB);

  // Check
  ok(SiteDataTestUtils.hasCookies(originA), "We have cookies for " + originA);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originA),
    "We have IDB for " + originA
  );
  ok(SiteDataTestUtils.hasCookies(originB), "We have cookies for " + originB);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originB),
    "We have IDB for " + originB
  );

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Check again
  ok(
    !SiteDataTestUtils.hasCookies(originA),
    "We should not have cookies for " + originA
  );
  ok(
    !(await SiteDataTestUtils.hasIndexedDB(originA)),
    "We should not have IDB for " + originA
  );
  ok(SiteDataTestUtils.hasCookies(originB), "We have cookies for " + originB);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originB),
    "We have IDB for " + originB
  );

  // Cleaning up permissions
  PermissionTestUtils.remove(originA, "cookie");
  PermissionTestUtils.remove(originB, "cookie");
});

// session only cookie life-time, 2 domains (sub.mozilla.org, www.mozilla.org),
// only the former has a cookie permission.
add_task(async function subDomains2() {
  info("Test subdomains and custom setting with cookieBehavior == 2");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
      ["browser.sanitizer.loglevel", "All"],
    ],
  });

  // Domains and data
  let originA = "https://sub.mozilla.org";
  PermissionTestUtils.add(
    originA,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  SiteDataTestUtils.addToCookies({ origin: originA });
  await SiteDataTestUtils.addToIndexedDB(originA);

  let originB = "https://www.mozilla.org";

  SiteDataTestUtils.addToCookies({ origin: originB });
  await SiteDataTestUtils.addToIndexedDB(originB);

  // Check
  ok(SiteDataTestUtils.hasCookies(originA), "We have cookies for " + originA);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originA),
    "We have IDB for " + originA
  );
  ok(SiteDataTestUtils.hasCookies(originB), "We have cookies for " + originB);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originB),
    "We have IDB for " + originB
  );

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Check again
  ok(SiteDataTestUtils.hasCookies(originA), "We have cookies for " + originA);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originA),
    "We have IDB for " + originA
  );
  ok(
    !SiteDataTestUtils.hasCookies(originB),
    "We should not have cookies for " + originB
  );
  ok(
    !(await SiteDataTestUtils.hasIndexedDB(originB)),
    "We should not have IDB for " + originB
  );

  // Cleaning up permissions
  PermissionTestUtils.remove(originA, "cookie");
});

// session only cookie life-time, 3 domains (sub.mozilla.org, www.mozilla.org, mozilla.org),
// only the former has a cookie permission. Both sub.mozilla.org and mozilla.org should
// be sustained.
add_task(async function subDomains3() {
  info(
    "Test base domain and subdomains and custom setting with cookieBehavior == 2"
  );

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
      ["browser.sanitizer.loglevel", "All"],
    ],
  });

  // Domains and data
  let originA = "https://sub.mozilla.org";
  PermissionTestUtils.add(
    originA,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );
  SiteDataTestUtils.addToCookies({ origin: originA });
  await SiteDataTestUtils.addToIndexedDB(originA);

  let originB = "https://mozilla.org";
  SiteDataTestUtils.addToCookies({ origin: originB });
  await SiteDataTestUtils.addToIndexedDB(originB);

  let originC = "https://www.mozilla.org";
  SiteDataTestUtils.addToCookies({ origin: originC });
  await SiteDataTestUtils.addToIndexedDB(originC);

  // Check
  ok(SiteDataTestUtils.hasCookies(originA), "We have cookies for " + originA);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originA),
    "We have IDB for " + originA
  );
  ok(SiteDataTestUtils.hasCookies(originB), "We have cookies for " + originB);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originB),
    "We have IDB for " + originB
  );
  ok(SiteDataTestUtils.hasCookies(originC), "We have cookies for " + originC);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originC),
    "We have IDB for " + originC
  );

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Check again
  ok(SiteDataTestUtils.hasCookies(originA), "We have cookies for " + originA);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originA),
    "We have IDB for " + originA
  );
  ok(SiteDataTestUtils.hasCookies(originB), "We have cookies for " + originB);
  ok(
    await SiteDataTestUtils.hasIndexedDB(originB),
    "We have IDB for " + originB
  );
  ok(
    !SiteDataTestUtils.hasCookies(originC),
    "We should not have cookies for " + originC
  );
  ok(
    !(await SiteDataTestUtils.hasIndexedDB(originC)),
    "We should not have IDB for " + originC
  );

  // Cleaning up permissions
  PermissionTestUtils.remove(originA, "cookie");
});
