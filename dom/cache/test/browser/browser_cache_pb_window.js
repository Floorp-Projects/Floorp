/* eslint-disable no-shadow */
var name = "pb-window-cache";

function testMatch(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise((resolve, reject) => {
      content.caches
        .match("http://foo.com")
        .then(function (response) {
          ok(true, "caches.match() should be successful");
          resolve();
        })
        .catch(function (err) {
          ok(false, "caches.match() should not throw error");
          reject();
        });
    });
  });
}

function testHas(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise(function (resolve, reject) {
      content.caches
        .has(name)
        .then(function (result) {
          ok(true, "caches.has() should be successful");
          resolve();
        })
        .catch(function (err) {
          ok(false, "caches.has() should not throw error");
          reject();
        });
    });
  });
}

function testOpen(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise(function (resolve, reject) {
      content.caches
        .open(name)
        .then(function (c) {
          ok(true, "caches.open() should be successful");
          resolve();
        })
        .catch(function (err) {
          ok(false, "caches.open() should not throw error");
          reject();
        });
    });
  });
}

function testDelete(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise(function (resolve, reject) {
      content.caches
        .delete(name)
        .then(function (result) {
          ok(true, "caches.delete() should be successful");
          resolve();
        })
        .catch(function (err) {
          ok(false, "caches.delete should not throw error");
          reject();
        });
    });
  });
}

function testKeys(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise(function (resolve, reject) {
      content.caches
        .keys()
        .then(function (names) {
          ok(true, "caches.keys() should be successful");
          resolve();
        })
        .catch(function (err) {
          ok(false, "caches.keys should not throw error");
          reject();
        });
    });
  });
}

function testOpen_worker(browser) {
  return SpecialPowers.spawn(browser, [], function () {
    let workerFunctionString = function () {
      caches.open("pb-worker-cache").then(
        function (cacheObject) {
          postMessage(cacheObject.toString());
        },
        function (reason) {
          postMessage(reason.name);
        }
      );
    }.toString();
    let workerBlobURL = content.URL.createObjectURL(
      new Blob(["(", workerFunctionString, ")()"], {
        type: "application/javascript",
      })
    );
    let worker = new content.Worker(workerBlobURL);
    content.URL.revokeObjectURL(workerBlobURL);
    return new Promise(function (resolve, reject) {
      worker.addEventListener("message", function (e) {
        let isGood = e.data != "SecurityError";
        ok(isGood, "caches.open() should be successful from worker");
        isGood ? resolve() : reject();
      });
    });
  });
}

function test() {
  let privateWin, privateTab;
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({
    set: [["dom.caches.testing.enabled", true]],
  })
    .then(() => {
      return BrowserTestUtils.openNewBrowserWindow({ private: true });
    })
    .then(pw => {
      privateWin = pw;
      privateTab = BrowserTestUtils.addTab(pw.gBrowser, "http://example.com/");
      return BrowserTestUtils.browserLoaded(privateTab.linkedBrowser);
    })
    .then(tab => {
      return Promise.all([
        testMatch(privateTab.linkedBrowser),
        testHas(privateTab.linkedBrowser),
        testOpen(privateTab.linkedBrowser),
        testDelete(privateTab.linkedBrowser),
        testKeys(privateTab.linkedBrowser),
        testOpen_worker(privateTab.linkedBrowser),
      ]);
    })
    .then(() => {
      return BrowserTestUtils.closeWindow(privateWin);
    })
    .then(finish);
}
