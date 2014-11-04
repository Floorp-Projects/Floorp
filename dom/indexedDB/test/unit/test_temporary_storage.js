/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.importGlobalProperties(['Blob']);

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  const urls = [
    { url: "http://www.alpha.com",        flags: [true, true, true, true] },
    { url: "http://www.beta.com",         flags: [true, false, false, false] },
    { url: "http://www.gamma.com",        flags: [true, true, false, false] },
    { url: "http://www.delta.com",        flags: [true, true, false, false] },
    { url: "http://www.epsilon.com",      flags: [true, true, false, false] },
    { url: "http://www2.alpha.com",       flags: [true, true, false, false] },
    { url: "http://www2.beta.com",        flags: [true, true, false, false] },
    { url: "http://www2.gamma.com",       flags: [true, true, false, false] },
    { url: "http://www2.delta.com",       flags: [true, true, true, false] },
    { url: "http://www2.epsilon.com",     flags: [true, true, true, true] },
    { url: "http://joe.blog.alpha.com",   flags: [true, true, true, true] },
    { url: "http://joe.blog.beta.com",    flags: [true, true, true, true] },
    { url: "http://joe.blog.gamma.com",   flags: [true, true, true, true] },
    { url: "http://joe.blog.delta.com",   flags: [true, true, true, true] },
    { url: "http://joe.blog.epsilon.com", flags: [true, true, true, true] },
    { url: "http://www.rudolf.org",       flags: [true, true, true, true] },
    { url: "http://www.pauline.org",      flags: [true, true, true, true] },
    { url: "http://www.marie.org",        flags: [true, true, true, true] },
    { url: "http://www.john.org",         flags: [true, true, true, true] },
    { url: "http://www.ema.org",          flags: [true, true, true, true] },
    { url: "http://www.trigger.com",      flags: [false, true, true, true] }
  ];
  const lastIndex = urls.length - 1;
  const lastUrl = urls[lastIndex].url;

  const openDBOptions = [
    { version: 1, storage: "temporary" },
    { version: 1 }
  ];

  let quotaManager =
    Components.classes["@mozilla.org/dom/quota/manager;1"]
              .getService(Components.interfaces.nsIQuotaManager);

  let ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  let dbSize = 0;

  function setLimit(limit) {
    if (limit) {
      SpecialPowers.setIntPref("dom.quotaManager.temporaryStorage.fixedLimit",
                               limit);
      return;
    }
    SpecialPowers.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
  }

  function getPrincipal(url) {
    let uri = ioService.newURI(url, null, null);
    return Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Components.interfaces.nsIScriptSecurityManager)
                     .getNoAppCodebasePrincipal(uri);
  }

  function getUsageForUrl(url, usageHandler) {
    let uri = ioService.newURI(url, null, null);
    function callback(uri, usage, fileUsage) {
      usageHandler(usage, fileUsage);
    }
    quotaManager.getUsageForURI(uri, callback);
  }

  function grabUsageAndContinueHandler(usage, fileUsage) {
    testGenerator.send(usage);
  }

  function checkUsage(stageIndex) {
    let handledIndex = 0;

    function usageHandler(usage, fileUsage) {
      let data = urls[handledIndex++];
      if (data.flags[stageIndex - 1]) {
        ok(usage > 0, "Non-zero usage for '" + data.url + "'");
      }
      else {
        ok(usage == 0, "Zero usage for '" + data.url + "'");
      }
      if (handledIndex == urls.length) {
        continueToNextStep();
      }
    }

    for (let i = 0; i < urls.length; i++) {
      getUsageForUrl(urls[i].url, usageHandler);
    }
  }

  // Enable clear() and test()
  let testingEnabled =
    SpecialPowers.getBoolPref("dom.quotaManager.testing");
  SpecialPowers.setBoolPref("dom.quotaManager.testing", true)

  // Calibration
  let request = indexedDB.openForPrincipal(getPrincipal(lastUrl), name,
                                           { storage: "temporary" });
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  getUsageForUrl(lastUrl, grabUsageAndContinueHandler);
  dbSize = yield undefined;

  for (let options of openDBOptions) {
    setLimit(lastIndex * dbSize / 1024);
    quotaManager.clear();

    info("Stage 1");

    let databases = [];
    for (let i = 0; i < lastIndex; i++) {
      let data = urls[i];

      info("Opening database for " + data.url);

      request = indexedDB.openForPrincipal(getPrincipal(data.url), name,
                                           options);
      request.onerror = errorHandler;
      request.onupgradeneeded = grabEventAndContinueHandler;
      request.onsuccess = grabEventAndContinueHandler;
      event = yield undefined;

      is(event.type, "upgradeneeded", "Got correct event type");

      let db = event.target.result;
      db.createObjectStore("foo", { });

      event = yield undefined;

      is(event.type, "success", "Got correct event type");

      databases.push(event.target.result);
    }

    request = indexedDB.openForPrincipal(getPrincipal(lastUrl), name, options);
    request.addEventListener("error", new ExpectError("QuotaExceededError"));
    request.onsuccess = unexpectedSuccessHandler;
    event = yield undefined;

    checkUsage(1);
    yield undefined;

    info("Stage 2");

    for (let i = 1; i < urls.length; i++) {
      databases[i] = null;

      scheduleGC();
      yield undefined;

      // The origin access time is set to the current system time when the first
      // database for an origin is registered or the last one is unregistered.
      // The registration happens when the database object is being created and
      // the unregistration when it is unlinked/garbage collected.
      // Some older windows systems have the system time limited to a maximum
      // resolution of 10 or 15 milliseconds, so without a pause here we would
      // end up with origins with the same access time which would cause random
      // failures.
      setTimeout(function() { testGenerator.next(); }, 20);
      yield undefined;
    }

    request = indexedDB.openForPrincipal(getPrincipal(lastUrl), name, options);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "upgradeneeded", "Got correct event type");

    let db = event.target.result;
    db.createObjectStore("foo", { });

    event = yield undefined;

    is(event.type, "success", "Got correct event type");

    checkUsage(2);
    yield undefined;

    info("Stage 3");

    setLimit(14 * dbSize / 1024);
    quotaManager.reset();

    request = indexedDB.openForPrincipal(getPrincipal(lastUrl), name, options);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Got correct event type");

    db = event.target.result;

    checkUsage(3);
    yield undefined;

    info("Stage 4");

    let trans = db.transaction(["foo"], "readwrite");

    let blob = new Blob(["bar"]);
    request = trans.objectStore("foo").add(blob, 42);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    trans.oncomplete = grabEventAndContinueHandler;
    event = yield undefined;

    checkUsage(4);
    yield undefined;
  }

  info("Cleanup");

  setLimit();
  quotaManager.reset();

  SpecialPowers.setBoolPref("dom.quotaManager.testing", testingEnabled);

  finishTest();
  yield undefined;
}
