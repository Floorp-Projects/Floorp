const BEHAVIOR_ACCEPT = 0;
const BEHAVIOR_REJECT_FOREIGN = 1;
const BEHAVIOR_REJECT = 2;
const BEHAVIOR_LIMIT_FOREIGN = 3;

const kPrefName = "network.cookie.cookieBehavior";

// Check if we are in frame, and declare ok and finishTest appropriately
const inFrame = ("" + location).match(/frame/);
if (inFrame) {
  ok = function(a, message) {
    if (!a) {
      parent.postMessage("FAILURE: " + message, "http://mochi.test:8888");
    } else {
      parent.postMessage(message, "http://mochi.test:8888");
    }
  };

  finishTest = function() {
    parent.postMessage("done", "http://mochi.test:8888");
  };
} else {
  finishTest = function() {
    SimpleTest.finish();
  };
}

function setCookieBehavior(behavior) {
  return SpecialPowers.pushPrefEnv({"set": [[kPrefName, behavior]]});
}

function runIFrame(url) {
  return new Promise((resolve, reject) => {
    function onMessage(e)  {
      if (e.data == "done") {
        resolve();
        window.removeEventListener('message', onMessage);
        return;
      }

      ok(!e.data.match(/^FAILURE/), e.data + " (IFRAME = " + url + ")");
    }
    window.addEventListener('message', onMessage);

    document.querySelector('iframe').src = url;
  });
}

function runWorker(url) {
  return new Promise((resolve, reject) => {
    var worker = new Worker(url);
    worker.addEventListener('message', function(e) {
      if (e.data == "done") {
        resolve();
        return;
      }

      ok(!e.data.match(/^FAILURE/), e.data + " (WORKER = " + url + ")");
    });
  });
}

function chromePower(allowed, blockSessionStorage) {

  // localStorage is affected by storage policy.
  try {
    SpecialPowers.wrap(window).localStorage.getItem("X");
    ok(allowed, "getting localStorage from chrome didn't throw");
  } catch (e) {
    ok(!allowed, "getting localStorage from chrome threw");
  }


  // sessionStorage is not. See bug 1183968.
  try {
    SpecialPowers.wrap(window).sessionStorage.getItem("X");
    ok(!blockSessionStorage, "getting sessionStorage from chrome didn't throw");
  } catch (e) {
    ok(blockSessionStorage, "getting sessionStorage from chrome threw");
  }

  // indexedDB is affected by storage policy.
  try {
    SpecialPowers.wrap(window).indexedDB;
    ok(allowed, "getting indexedDB from chrome didn't throw");
  } catch (e) {
    ok(!allowed, "getting indexedDB from chrome threw");
  }

  // Same with caches, along with the additional https-only requirement.
  try {
    var shouldResolve = allowed && location.protocol == "https:";
    var promise = SpecialPowers.wrap(window).caches.keys();
    ok(true, "getting caches from chrome should never throw");
    return new Promise((resolve, reject) => {
      promise.then(function() {
        ok(shouldResolve, "The promise was resolved for chrome");
        resolve();
      }, function(e) {
        ok(!shouldResolve, "The promise was rejected for chrome: " + e);
        resolve();
      });
    });
  } catch (e) {
    ok(false, "getting caches from chrome threw");
  }
}

function storageAllowed() {
  try {
    localStorage.getItem("X");
    ok(true, "getting localStorage didn't throw");
  } catch (e) {
    ok(false, "getting localStorage should not throw");
  }

  try {
    sessionStorage.getItem("X");
    ok(true, "getting sessionStorage didn't throw");
  } catch (e) {
    ok(false, "getting sessionStorage should not throw");
  }

  try {
    indexedDB;
    ok(true, "getting indexedDB didn't throw");
  } catch (e) {
    ok(false, "getting indexedDB should not throw");
  }

  try {
    var promise = caches.keys();
    ok(true, "getting caches didn't throw");

    return new Promise((resolve, reject) => {
      promise.then(function() {
        ok(location.protocol == "https:", "The promise was not rejected");
        resolve();
      }, function() {
        ok(location.protocol != "https:", "The promise should not have been rejected");
        resolve();
      });
    });
  } catch (e) {
    ok(false, "getting caches should not have thrown");
    return Promise.resolve();
  }
}

function storagePrevented() {
  try {
    localStorage.getItem("X");
    ok(false, "getting localStorage should have thrown");
  } catch (e) {
    ok(true, "getting localStorage threw");
  }

  if (location.hash == "#thirdparty") {
    // No matter what the user's preferences are, we don't block
    // sessionStorage in 3rd-party iframes. We do block them everywhere
    // else however.
    try {
      sessionStorage.getItem("X");
      ok(true, "getting sessionStorage didn't throw");
    } catch (e) {
      ok(false, "getting sessionStorage should not have thrown");
    }
  } else {
    try {
      sessionStorage.getItem("X");
      ok(false, "getting sessionStorage should have thrown");
    } catch (e) {
      ok(true, "getting sessionStorage threw");
    }
  }

  try {
    indexedDB;
    ok(false, "getting indexedDB should have thrown");
  } catch (e) {
    ok(true, "getting indexedDB threw");
  }

  try {
    var promise = caches.keys();
    ok(true, "getting caches didn't throw");

    return new Promise((resolve, reject) => {
      promise.then(function() {
        ok(false, "The promise should have rejected");
        resolve();
      }, function() {
        ok(true, "The promise was rejected");
        resolve();
      });
    });
  } catch (e) {
    ok(false, "getting caches should not have thrown");

    return Promise.resolve();
  }
}

function task(fn) {
  if (!inFrame) {
    SimpleTest.waitForExplicitFinish();
  }

  var gen = fn();

  function next_step(val, e) {
    var it;
    try {
      if (typeof e !== "undefined") {
        it = gen.throw(e);
      } else {
        it = gen.next(val);
      }
    } catch (e) {
      ok(false, "An error was thrown while stepping: " + e);
      ok(false, "Stack: " + e.stack);
      finishTest();
    }

    if (it.done) {
      finishTest();
      return;
    }
    it.value.then(next_step, (e) => next_step(null, e));
  }

  if (!gen.then) {
    next_step();
  } else {
    gen.then(finishTest, e => {
      ok(false, "An error was thrown while stepping: " + e);
      ok(false, "Stack: " + e.stack);
      finishTest();
    });
  }
}

var thirdparty = "https://example.com/tests/dom/tests/mochitest/general/";
