// Bug 380852 - Delete permission manager entries in Clear Recent History

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});

XPCOMUtils.defineLazyServiceGetter(this, "sas",
                                   "@mozilla.org/storage/activity-service;1",
                                   "nsIStorageActivityService");
XPCOMUtils.defineLazyServiceGetter(this, "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");
XPCOMUtils.defineLazyServiceGetter(this, "quotaManagerService",
                                   "@mozilla.org/dom/quota-manager-service;1",
                                   "nsIQuotaManagerService");

const oneHour = 3600000000;
const fiveHours = oneHour * 5;

const itemsToClear = [ "cookies", "offlineApps" ];

function waitForUnregister(host) {
  return new Promise(resolve => {
    let listener = {
      onUnregister: registration => {
        if (registration.principal.URI.host != host) {
          return;
        }
        let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
                    .getService(Ci.nsIServiceWorkerManager);
        swm.removeListener(listener);
        resolve(registration);
      }
    };
    swm.addListener(listener);
  });
}

async function createData(host) {
  let pageURL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://" + host) + "sanitize.html";

  return BrowserTestUtils.withNewTab(pageURL, async function(browser) {
    await ContentTask.spawn(browser, null, () => {
      return new content.window.Promise(resolve => {
        let id = content.window.setInterval(() => {
          if ("foobar" in content.window.localStorage) {
            content.window.clearInterval(id);
            resolve(true);
          }
        }, 1000);
      });
    });
  });
}

function moveOriginInTime(principals, endDate, host) {
  for (let i = 0; i < principals.length; ++i) {
    let principal = principals.queryElementAt(i, Ci.nsIPrincipal);
    if (principal.URI.host == host) {
      sas.moveOriginInTime(principal, endDate - fiveHours);
      return true;
    }
  }
  return false;
}

async function getData(host) {
  let dummyURL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://" + host) + "dummy_page.html";

  // LocalStorage + IndexedDB
  let data = await BrowserTestUtils.withNewTab(dummyURL, async function(browser) {
    return ContentTask.spawn(browser, null, () => {
      return new content.window.Promise(resolve => {
        let obj = {
          localStorage: "foobar" in content.window.localStorage,
          indexedDB: true,
          serviceWorker: false,
        };

        let request = content.window.indexedDB.open("sanitizer_test", 1);
        request.onupgradeneeded = event => {
          obj.indexedDB = false;
        };
        request.onsuccess = event => {
          resolve(obj);
        };
      });
    });
  });

  // ServiceWorkers
  let serviceWorkers = swm.getAllRegistrations();
  for (let i = 0; i < serviceWorkers.length; i++) {
    let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    if (sw.principal.URI.host == host) {
      data.serviceWorker = true;
      break;
    }
  }

  return data;
}

add_task(async function testWithRange() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.testing.enabled", true]
  ]});

  // The service may have picked up activity from prior tests in this run.
  // Clear it.
  sas.testOnlyReset();

  let endDate = Date.now() * 1000;
  let principals = sas.getActiveOrigins(endDate - oneHour, endDate);
  is(principals.length, 0, "starting from clear activity state");

  info("sanitize: " + itemsToClear.join(", "));
  await Sanitizer.sanitize(itemsToClear, {ignoreTimespan: false});

  await createData("example.org");
  await createData("example.com");

  endDate = Date.now() * 1000;
  principals = sas.getActiveOrigins(endDate - oneHour, endDate);
  ok(!!principals, "We have an active origin.");
  ok(principals.length >= 2, "We have an active origin.");

  let found = 0;
  for (let i = 0; i < principals.length; ++i) {
    let principal = principals.queryElementAt(i, Ci.nsIPrincipal);
    if (principal.URI.host == "example.org" ||
        principal.URI.host == "example.com") {
      found++;
    }
  }

  is(found, 2, "Our origins are active.");

  let dataPre = await getData("example.org");
  ok(dataPre.localStorage, "We have localStorage data");
  ok(dataPre.indexedDB, "We have indexedDB data");
  ok(dataPre.serviceWorker, "We have serviceWorker data");

  dataPre = await getData("example.com");
  ok(dataPre.localStorage, "We have localStorage data");
  ok(dataPre.indexedDB, "We have indexedDB data");
  ok(dataPre.serviceWorker, "We have serviceWorker data");

  // Let's move example.com in the past.
  ok(moveOriginInTime(principals, endDate, "example.com"), "Operation completed!");

  let p = waitForUnregister("example.org");

  // Clear it
  info("sanitize: " + itemsToClear.join(", "));
  await Sanitizer.sanitize(itemsToClear, {ignoreTimespan: false});
  await p;

  let dataPost = await getData("example.org");
  ok(!dataPost.localStorage, "We don't have localStorage data");
  ok(!dataPost.indexedDB, "We don't have indexedDB data");
  ok(!dataPost.serviceWorker, "We don't have serviceWorker data");

  dataPost = await getData("example.com");
  ok(dataPost.localStorage, "We still have localStorage data");
  ok(dataPost.indexedDB, "We still have indexedDB data");
  ok(dataPost.serviceWorker, "We still have serviceWorker data");

  // We have to move example.com in the past because how we check IDB triggers
  // a storage activity.
  ok(moveOriginInTime(principals, endDate, "example.com"), "Operation completed!");

  // Let's call the clean up again.
  info("sanitize again to ensure clearing doesn't expand the activity scope");
  await Sanitizer.sanitize(itemsToClear, {ignoreTimespan: false});

  dataPost = await getData("example.com");
  ok(dataPost.localStorage, "We still have localStorage data");
  ok(dataPost.indexedDB, "We still have indexedDB data");
  ok(dataPost.serviceWorker, "We still have serviceWorker data");

  dataPost = await getData("example.org");
  ok(!dataPost.localStorage, "We don't have localStorage data");
  ok(!dataPost.indexedDB, "We don't have indexedDB data");
  ok(!dataPost.serviceWorker, "We don't have serviceWorker data");

  sas.testOnlyReset();
});
