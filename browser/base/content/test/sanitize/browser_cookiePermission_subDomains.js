const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm");
const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm");

// 2 domains: www.mozilla.org (session-only) mozilla.org (allowed) - after the
// cleanp, mozilla.org must have data.
add_task(async function subDomains() {
  info("Test subdomains and custom setting");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_NORMALLY ],
    ["browser.sanitizer.loglevel", "All"],
  ]});

  // Domains and data
  let uriA = Services.io.newURI("https://www.mozilla.org");
  Services.perms.add(uriA, "cookie", Ci.nsICookiePermission.ACCESS_SESSION);

  Services.cookies.add(uriA.host, "/test", "a", "b",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);

  await createIndexedDB(uriA.host, {});

  let uriB = Services.io.newURI("https://mozilla.org");
  Services.perms.add(uriB, "cookie", Ci.nsICookiePermission.ACCESS_ALLOW);

  Services.cookies.add(uriB.host, "/test", "c", "d",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);

  await createIndexedDB(uriB.host, {});

  // Check
  ok(await checkCookie(uriA.host, {}), "We have cookies for URI: " + uriA.host);
  ok(await checkIndexedDB(uriA.host, {}), "We have IDB for URI: " + uriA.host);
  ok(await checkCookie(uriB.host, {}), "We have cookies for URI: " + uriB.host);
  ok(await checkIndexedDB(uriB.host, {}), "We have IDB for URI: " + uriB.host);

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Check again
  ok(!(await checkCookie(uriA.host, {})), "We should not have cookies for URI: " + uriA.host);
  ok(!(await checkIndexedDB(uriA.host, {})), "We should not have IDB for URI: " + uriA.host);
  ok(await checkCookie(uriB.host, {}), "We should have cookies for URI: " + uriB.host);
  ok(await checkIndexedDB(uriB.host, {}), "We should have IDB for URI: " + uriB.host);

  // Cleaning up permissions
  Services.perms.remove(uriA, "cookie");
  Services.perms.remove(uriB, "cookie");
});

// session only cookie life-time, 2 domains (mozilla.org, www.mozilla.org),
// only the latter has a cookie permission.
add_task(async function subDomains() {
  info("Test subdomains and custom setting with cookieBehavior == 2");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION ],
    ["browser.sanitizer.loglevel", "All"],
  ]});

  // Domains and data
  let uriA = Services.io.newURI("https://sub.mozilla.org");
  Services.perms.add(uriA, "cookie", Ci.nsICookiePermission.ACCESS_ALLOW);

  Services.cookies.add(uriA.host, "/test", "a", "b",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);

  await createIndexedDB(uriA.host, {});

  let uriB = Services.io.newURI("https://www.mozilla.org");

  Services.cookies.add(uriB.host, "/test", "c", "d",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);

  await createIndexedDB(uriB.host, {});

  // Check
  ok(await checkCookie(uriA.host, {}), "We have cookies for URI: " + uriA.host);
  ok(await checkIndexedDB(uriA.host, {}), "We have IDB for URI: " + uriA.host);
  ok(await checkCookie(uriB.host, {}), "We have cookies for URI: " + uriB.host);
  ok(await checkIndexedDB(uriB.host, {}), "We have IDB for URI: " + uriB.host);

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Check again
  ok(await checkCookie(uriA.host, {}), "We should have cookies for URI: " + uriA.host);
  ok(await checkIndexedDB(uriA.host, {}), "We should have IDB for URI: " + uriA.host);
  ok(!await checkCookie(uriB.host, {}), "We should not have cookies for URI: " + uriB.host);
  ok(!await checkIndexedDB(uriB.host, {}), "We should not have IDB for URI: " + uriB.host);

  // Cleaning up permissions
  Services.perms.remove(uriA, "cookie");
});
