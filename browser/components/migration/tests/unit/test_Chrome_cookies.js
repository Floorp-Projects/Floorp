"use strict";

const { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);

add_task(async function() {
  registerFakePath("ULibDir", do_get_file("Library/"));
  let migrator = await MigrationUtils.getMigrator("chrome");

  Assert.ok(
    await migrator.isSourceAvailable(),
    "Sanity check the source exists"
  );

  const COOKIE = {
    expiry: 2145934800,
    host: "unencryptedcookie.invalid",
    isHttpOnly: false,
    isSession: false,
    name: "testcookie",
    path: "/",
    value: "testvalue",
  };

  // Sanity check.
  Assert.equal(
    Services.cookies.countCookiesFromHost(COOKIE.host),
    0,
    "There are no cookies initially"
  );

  const PROFILE = {
    id: "Default",
    name: "Person 1",
  };

  // Migrate unencrypted cookies.
  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.COOKIES,
    PROFILE
  );

  Assert.equal(
    Services.cookies.countCookiesFromHost(COOKIE.host),
    1,
    "Migrated the expected number of unencrypted cookies"
  );
  Assert.equal(
    Services.cookies.countCookiesFromHost("encryptedcookie.invalid"),
    0,
    "Migrated the expected number of encrypted cookies"
  );

  // Now check the cookie details.
  let cookies = Services.cookies.getCookiesFromHost(COOKIE.host, {});
  Assert.ok(cookies.length, "Cookies available");
  let foundCookie = cookies[0];

  for (let prop of Object.keys(COOKIE)) {
    Assert.equal(foundCookie[prop], COOKIE[prop], "Check cookie " + prop);
  }

  // Cleanup.
  await ForgetAboutSite.removeDataFromDomain(COOKIE.host);
  Assert.equal(
    Services.cookies.countCookiesFromHost(COOKIE.host),
    0,
    "There are no cookies after cleanup"
  );
});
