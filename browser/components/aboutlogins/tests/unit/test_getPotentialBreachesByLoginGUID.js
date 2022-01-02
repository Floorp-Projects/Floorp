/**
 * Test LoginBreaches.getPotentialBreachesByLoginGUID
 */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Initializing BrowserGlue requires a profile on Windows.
do_get_profile();

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

ChromeUtils.defineModuleGetter(
  this,
  "LoginBreaches",
  "resource:///modules/LoginBreaches.jsm"
);

const TEST_BREACHES = [
  {
    AddedDate: "2018-12-20T23:56:26Z",
    BreachDate: "2018-12-16",
    Domain: "breached.com",
    Name: "Breached",
    PwnCount: 1643100,
    DataClasses: ["Email addresses", "Usernames", "Passwords", "IP addresses"],
    _status: "synced",
    id: "047940fe-d2fd-4314-b636-b4a952ee0043",
    last_modified: "1541615610052",
    schema: "1541615609018",
  },
  {
    AddedDate: "2018-12-20T23:56:26Z",
    BreachDate: "2018-12-16",
    Domain: "breached-subdomain.host.com",
    Name: "Only a Sub-Domain was Breached",
    PwnCount: 2754200,
    DataClasses: ["Email addresses", "Usernames", "Passwords", "IP addresses"],
    _status: "synced",
    id: "047940fe-d2fd-4314-b636-b4a952ee0044",
    last_modified: "1541615610052",
    schema: "1541615609018",
  },
  {
    AddedDate: "2018-12-20T23:56:26Z",
    BreachDate: "2018-12-16",
    Domain: "breached-site-without-passwords.com",
    Name: "Breached Site without passwords",
    PwnCount: 987654,
    DataClasses: ["Email addresses", "Usernames", "IP addresses"],
    _status: "synced",
    id: "047940fe-d2fd-4314-b636-b4a952ee0045",
    last_modified: "1541615610052",
    schema: "1541615609018",
  },
];

const NOT_BREACHED_LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://www.example.com",
  formActionOrigin: "https://www.example.com",
  username: "username",
  password: "password",
  timePasswordChanged: new Date("2018-12-15").getTime(),
});
const BREACHED_LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://www.breached.com",
  formActionOrigin: "https://www.breached.com",
  username: "username",
  password: "password",
  timePasswordChanged: new Date("2018-12-15").getTime(),
});
const NOT_BREACHED_SUBDOMAIN_LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://not-breached-subdomain.host.com",
  formActionOrigin: "https://not-breached-subdomain.host.com",
  username: "username",
  password: "password",
});
const BREACHED_SUBDOMAIN_LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://breached-subdomain.host.com",
  formActionOrigin: "https://breached-subdomain.host.com",
  username: "username",
  password: "password",
  timePasswordChanged: new Date("2018-12-15").getTime(),
});
const LOGIN_FOR_BREACHED_SITE_WITHOUT_PASSWORDS = LoginTestUtils.testData.formLogin(
  {
    origin: "https://breached-site-without-passwords.com",
    formActionOrigin: "https://breached-site-without-passwords.com",
    username: "username",
    password: "password",
    timePasswordChanged: new Date("2018-12-15").getTime(),
  }
);
const LOGIN_WITH_NON_STANDARD_URI = LoginTestUtils.testData.formLogin({
  origin: "someApp://random/path/to/login",
  formActionOrigin: "someApp://random/path/to/login",
  username: "username",
  password: "password",
  timePasswordChanged: new Date("2018-12-15").getTime(),
});

add_task(async function test_notBreachedLogin() {
  Services.logins.addLogin(NOT_BREACHED_LOGIN);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [NOT_BREACHED_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    0,
    "Should be 0 breached logins."
  );
});

add_task(async function test_breachedLogin() {
  Services.logins.addLogin(BREACHED_LOGIN);
  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [NOT_BREACHED_LOGIN, BREACHED_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    1,
    "Should be 1 breached login: " + BREACHED_LOGIN.origin
  );
  Assert.strictEqual(
    breachesByLoginGUID.get(BREACHED_LOGIN.guid).breachAlertURL,
    "https://monitor.firefox.com/breach-details/Breached?utm_source=firefox-desktop&utm_medium=referral&utm_campaign=about-logins&utm_content=about-logins",
    "Breach alert link should be equal to the breachAlertURL"
  );
});

add_task(async function test_notBreachedSubdomain() {
  Services.logins.addLogin(NOT_BREACHED_SUBDOMAIN_LOGIN);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [NOT_BREACHED_LOGIN, NOT_BREACHED_SUBDOMAIN_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    0,
    "Should be 0 breached logins."
  );
});

add_task(async function test_breachedSubdomain() {
  Services.logins.addLogin(BREACHED_SUBDOMAIN_LOGIN);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [NOT_BREACHED_SUBDOMAIN_LOGIN, BREACHED_SUBDOMAIN_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    1,
    "Should be 1 breached login: " + BREACHED_SUBDOMAIN_LOGIN.origin
  );
});

add_task(async function test_breachedSiteWithoutPasswords() {
  Services.logins.addLogin(LOGIN_FOR_BREACHED_SITE_WITHOUT_PASSWORDS);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [LOGIN_FOR_BREACHED_SITE_WITHOUT_PASSWORDS],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    0,
    "Should be 0 breached login: " +
      LOGIN_FOR_BREACHED_SITE_WITHOUT_PASSWORDS.origin
  );
});

add_task(async function test_breachAlertHiddenAfterDismissal() {
  BREACHED_LOGIN.guid = "{d2de5ac1-4de6-e544-a7af-1f75abcba92b}";

  await Services.logins.initializationPromise;
  const storageJSON = Services.logins.wrappedJSObject._storage.wrappedJSObject;

  storageJSON.recordBreachAlertDismissal(BREACHED_LOGIN.guid);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [BREACHED_LOGIN, NOT_BREACHED_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    0,
    "Should be 0 breached logins after dismissal: " + BREACHED_LOGIN.origin
  );

  info("Clear login storage");
  Services.logins.removeAllUserFacingLogins();

  const breachesByLoginGUID2 = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [BREACHED_LOGIN, NOT_BREACHED_LOGIN],
    TEST_BREACHES
  );
  Assert.strictEqual(
    breachesByLoginGUID2.size,
    1,
    "Breached login should re-appear after clearing storage: " +
      BREACHED_LOGIN.origin
  );
});

add_task(async function test_newBreachAfterDismissal() {
  TEST_BREACHES[0].AddedDate = new Date().toISOString();

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [BREACHED_LOGIN, NOT_BREACHED_LOGIN],
    TEST_BREACHES
  );

  Assert.strictEqual(
    breachesByLoginGUID.size,
    1,
    "Should be 1 breached login after new breach following the dismissal of a previous breach: " +
      BREACHED_LOGIN.origin
  );
});

add_task(async function test_ExceptionsThrownByNonStandardURIsAreCaught() {
  Services.logins.addLogin(LOGIN_WITH_NON_STANDARD_URI);

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [LOGIN_WITH_NON_STANDARD_URI, BREACHED_LOGIN],
    TEST_BREACHES
  );

  Assert.strictEqual(
    breachesByLoginGUID.size,
    1,
    "Exceptions thrown by logins with non-standard URIs should be caught."
  );
});

add_task(async function test_setBreachesFromRemoteSettingsSync() {
  const login = NOT_BREACHED_SUBDOMAIN_LOGIN;
  const nowExampleIsInBreachedRecords = [
    {
      AddedDate: "2018-12-20T23:56:26Z",
      BreachDate: "2018-12-16",
      Domain: "not-breached-subdomain.host.com",
      Name: "not-breached-subdomain.host.com is now breached!",
      PwnCount: 1643100,
      DataClasses: [
        "Email addresses",
        "Usernames",
        "Passwords",
        "IP addresses",
      ],
      _status: "synced",
      id: "047940fe-d2fd-4314-b636-b4a952ee0044",
      last_modified: "1541615610052",
      schema: "1541615609018",
    },
  ];
  async function emitSync() {
    await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).emit(
      "sync",
      { data: { current: nowExampleIsInBreachedRecords } }
    );
  }

  const beforeSyncBreachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [login]
  );
  Assert.strictEqual(
    beforeSyncBreachesByLoginGUID.size,
    0,
    "Should be 0 breached login before not-breached-subdomain.host.com is added to fxmonitor-breaches collection and synced: "
  );
  gBrowserGlue.observe(null, "browser-glue-test", "add-breaches-sync-handler");
  const db = await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).db;
  await db.importChanges({}, 42, [nowExampleIsInBreachedRecords[0]]);
  await emitSync();

  const breachesByLoginGUID = await LoginBreaches.getPotentialBreachesByLoginGUID(
    [login]
  );
  Assert.strictEqual(
    breachesByLoginGUID.size,
    1,
    "Should be 1 breached login after not-breached-subdomain.host.com is added to fxmonitor-breaches collection and synced: "
  );
});
