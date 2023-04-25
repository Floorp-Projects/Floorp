/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);
const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const oneHour = 3600000000;

add_task(async function sanitizeWithExceptionsOnShutdown() {
  info(
    "Test that cookies that are marked as allowed from the user do not get \
    cleared when cleaning on shutdown is done"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sanitizer.loglevel", "All"],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });

  // Clean up before start
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  let originALLOW = "https://mozilla.org";
  PermissionTestUtils.add(
    originALLOW,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  let originDENY = "https://example123.com";
  PermissionTestUtils.add(
    originDENY,
    "cookie",
    Ci.nsICookiePermission.ACCESS_DENY
  );

  SiteDataTestUtils.addToCookies({ origin: originALLOW });
  ok(
    SiteDataTestUtils.hasCookies(originALLOW),
    "We have cookies for " + originALLOW
  );

  SiteDataTestUtils.addToCookies({ origin: originDENY });
  ok(
    SiteDataTestUtils.hasCookies(originDENY),
    "We have cookies for " + originDENY
  );

  await Sanitizer.runSanitizeOnShutdown();

  ok(
    SiteDataTestUtils.hasCookies(originALLOW),
    "We should have cookies for " + originALLOW
  );

  ok(
    !SiteDataTestUtils.hasCookies(originDENY),
    "We should not have cookies for " + originDENY
  );
});

add_task(async function sanitizeNoExceptionsInTimeRange() {
  info(
    "Test that no exceptions are made when not clearing on shutdown, e.g. clearing within a range"
  );

  // Clean up before start
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  let originALLOW = "https://mozilla.org";
  PermissionTestUtils.add(
    originALLOW,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  let originDENY = "https://bar123.com";
  PermissionTestUtils.add(
    originDENY,
    "cookie",
    Ci.nsICookiePermission.ACCESS_DENY
  );

  SiteDataTestUtils.addToCookies({ origin: originALLOW });
  ok(
    SiteDataTestUtils.hasCookies(originALLOW),
    "We have cookies for " + originALLOW
  );

  SiteDataTestUtils.addToCookies({ origin: originDENY });
  ok(
    SiteDataTestUtils.hasCookies(originDENY),
    "We have cookies for " + originDENY
  );

  let to = Date.now() * 1000;
  let from = to - oneHour;
  await Sanitizer.sanitize(["cookies"], { range: [from, to] });

  ok(
    !SiteDataTestUtils.hasCookies(originALLOW),
    "We should not have cookies for " + originALLOW
  );

  ok(
    !SiteDataTestUtils.hasCookies(originDENY),
    "We should not have cookies for " + originDENY
  );
});

add_task(async function sanitizeWithExceptionsOnStartup() {
  info(
    "Test that cookies that are marked as allowed from the user do not get \
    cleared when cleaning on startup is done, for example after a crash"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sanitizer.loglevel", "All"],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });

  // Clean up before start
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  let originALLOW = "https://mozilla.org";
  PermissionTestUtils.add(
    originALLOW,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  let originDENY = "https://example123.com";
  PermissionTestUtils.add(
    originDENY,
    "cookie",
    Ci.nsICookiePermission.ACCESS_DENY
  );

  SiteDataTestUtils.addToCookies({ origin: originALLOW });
  ok(
    SiteDataTestUtils.hasCookies(originALLOW),
    "We have cookies for " + originALLOW
  );

  SiteDataTestUtils.addToCookies({ origin: originDENY });
  ok(
    SiteDataTestUtils.hasCookies(originDENY),
    "We have cookies for " + originDENY
  );

  let pendingSanitizations = [
    {
      id: "shutdown",
      itemsToClear: ["cookies"],
      options: {},
    },
  ];
  Services.prefs.setBoolPref(Sanitizer.PREF_SANITIZE_ON_SHUTDOWN, true);
  Services.prefs.setStringPref(
    Sanitizer.PREF_PENDING_SANITIZATIONS,
    JSON.stringify(pendingSanitizations)
  );

  await Sanitizer.onStartup();

  ok(
    SiteDataTestUtils.hasCookies(originALLOW),
    "We should have cookies for " + originALLOW
  );

  ok(
    !SiteDataTestUtils.hasCookies(originDENY),
    "We should not have cookies for " + originDENY
  );
});

add_task(async function sanitizeWithSessionExceptionsOnShutdown() {
  info(
    "Test that cookies that are marked as allowed on session is cleared when sanitizeOnShutdown is false"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sanitizer.loglevel", "All"],
      ["privacy.sanitize.sanitizeOnShutdown", false],
    ],
  });

  // Clean up before start
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  let originAllowSession = "https://mozilla.org";
  PermissionTestUtils.add(
    originAllowSession,
    "cookie",
    Ci.nsICookiePermission.ACCESS_SESSION
  );

  SiteDataTestUtils.addToCookies({ origin: originAllowSession });
  ok(
    SiteDataTestUtils.hasCookies(originAllowSession),
    "We have cookies for " + originAllowSession
  );

  await Sanitizer.runSanitizeOnShutdown();

  ok(
    !SiteDataTestUtils.hasCookies(originAllowSession),
    "We should not have cookies for " + originAllowSession
  );
});

add_task(async function sanitizeWithManySessionExceptionsOnShutdown() {
  info(
    "Test that lots of allowed on session exceptions are cleared when sanitizeOnShutdown is false"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.sanitize.sanitizeOnShutdown", false],
      ["dom.quotaManager.backgroundTask.enabled", true],
    ],
  });

  // Clean up before start
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  info("Setting cookies");

  const origins = new Array(300)
    .fill(0)
    .map((v, i) => `https://mozilla${i}.org`);

  for (const origin of origins) {
    PermissionTestUtils.add(
      origin,
      "cookie",
      Ci.nsICookiePermission.ACCESS_SESSION
    );
    SiteDataTestUtils.addToCookies({ origin });
  }

  ok(
    origins.every(origin => SiteDataTestUtils.hasCookies(origin)),
    "All origins have cookies"
  );

  info("Running sanitization");

  await Sanitizer.runSanitizeOnShutdown();

  ok(
    origins.every(origin => !SiteDataTestUtils.hasCookies(origin)),
    "All origins lost cookies"
  );
});
