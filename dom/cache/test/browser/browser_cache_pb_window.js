/* eslint-disable no-shadow */
var name = "pb-window-cache";

function testMatch(browser) {
  return SpecialPowers.spawn(browser, [name], function (name) {
    return new Promise((resolve, reject) => {
      content.caches
        .match("http://foo.com")
        .then(function (response) {
          ok(false, "caches.match() should not return success");
          reject();
        })
        .catch(function (err) {
          is(
            "SecurityError",
            err.name,
            "caches.match() should throw SecurityError"
          );
          resolve();
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
          ok(false, "caches.has() should not return success");
          reject();
        })
        .catch(function (err) {
          is(
            "SecurityError",
            err.name,
            "caches.has() should throw SecurityError"
          );
          resolve();
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
          ok(false, "caches.open() should not return success");
          reject();
        })
        .catch(function (err) {
          is(
            "SecurityError",
            err.name,
            "caches.open() should throw SecurityError"
          );
          resolve();
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
          ok(false, "caches.delete() should not return success");
          reject();
        })
        .catch(function (err) {
          is(
            "SecurityError",
            err.name,
            "caches.delete() should throw SecurityError"
          );
          resolve();
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
          ok(false, "caches.keys() should not return success");
          reject();
        })
        .catch(function (err) {
          is(
            "SecurityError",
            err.name,
            "caches.keys() should throw SecurityError"
          );
          resolve();
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
        let isGood = e.data === "SecurityError";
        ok(isGood, "caches.open() should throw SecurityError from worker");
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
