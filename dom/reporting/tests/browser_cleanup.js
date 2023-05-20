/* eslint-disable mozilla/no-arbitrary-setTimeout */

const TEST_HOST = "example.org";
const TEST_DOMAIN = "https://" + TEST_HOST;
const TEST_PATH = "/browser/dom/reporting/tests/";
const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "empty.html";
const TEST_SJS = TEST_DOMAIN + TEST_PATH + "delivering.sjs";

async function storeReportingHeader(browser, extraParams = "") {
  await SpecialPowers.spawn(
    browser,
    [{ url: TEST_SJS, extraParams }],
    async obj => {
      await content
        .fetch(
          obj.url +
            "?task=header" +
            (obj.extraParams.length ? "&" + obj.extraParams : "")
        )
        .then(r => r.text())
        .then(text => {
          is(text, "OK", "Report-to header sent");
        });
    }
  );
}

add_task(async function () {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.reporting.enabled", true],
      ["dom.reporting.header.enabled", true],
      ["dom.reporting.testing.enabled", true],
      ["dom.reporting.delivering.timeout", 1],
      ["dom.reporting.cleanup.timeout", 1],
      ["privacy.userContext.enabled", true],
    ],
  });
});

add_task(async function () {
  info("Testing a total cleanup");

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser);
  ok(ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN), "We have data");

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before a full cleanup"
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Testing a total QuotaManager cleanup");

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser);
  ok(ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN), "We have data");

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_REPORTS, value =>
      resolve()
    );
  });

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before a reports cleanup"
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Testing a QuotaManager host cleanup");

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser);
  ok(ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN), "We have data");

  await new Promise(resolve => {
    Services.clearData.deleteDataFromHost(
      TEST_HOST,
      true,
      Ci.nsIClearDataService.CLEAR_REPORTS,
      value => resolve()
    );
  });

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before a reports cleanup"
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Testing a 410 endpoint status");

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser, "410=true");
  ok(ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN), "We have data");

  await SpecialPowers.spawn(browser, [], async _ => {
    let testingInterface = new content.TestingDeprecatedInterface();
    ok(!!testingInterface, "Created a deprecated interface");
  });

  await new Promise((resolve, reject) => {
    let count = 0;
    let id = setInterval(_ => {
      ++count;
      if (count > 10) {
        ok(false, "Something went wrong.");
        clearInterval(id);
        reject();
      }

      if (!ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN)) {
        ok(true, "No data after a 410!");
        clearInterval(id);
        resolve();
      }
    }, 1000);
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Creating a new container");

  let identity = ContextualIdentityService.create(
    "Report-To-Test",
    "fingerprint",
    "orange"
  );

  info("Creating a new container tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE, {
    userContextId: identity.userContextId,
  });
  is(
    tab.getAttribute("usercontextid"),
    "" + identity.userContextId,
    "New tab has the right UCI"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser);
  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "We don't have data for the origin"
  );
  ok(
    ChromeUtils.hasReportingHeaderForOrigin(
      TEST_DOMAIN + "^userContextId=" + identity.userContextId
    ),
    "We have data for the origin + userContextId"
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  ContextualIdentityService.remove(identity.userContextId);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(
      TEST_DOMAIN + "^userContextId=" + identity.userContextId
    ),
    "No more data after a container removal"
  );
});

add_task(async function () {
  info("TTL cleanup");

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(
    !ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "No data before the test"
  );

  await storeReportingHeader(browser);
  ok(
    ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN),
    "We have data for the origin"
  );

  // Let's wait a bit.
  await new Promise(resolve => {
    setTimeout(resolve, 5000);
  });

  ok(!ChromeUtils.hasReportingHeaderForOrigin(TEST_DOMAIN), "No data anymore");

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
