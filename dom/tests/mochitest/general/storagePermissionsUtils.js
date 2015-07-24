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
    return new Promise((resolve, reject) => {
        SpecialPowers.pushPrefEnv({"set": [[kPrefName, behavior]]}, resolve);
    });
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
    window.addEventListener('message', onMessage, false);

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

function chromePower() {
  try {
    SpecialPowers.wrap(window).localStorage.getItem("X");
    ok(true, "getting localStorage didn't throw");
  } catch (e) {
    ok(false, "getting localStorage should not throw");
  }

  try {
    SpecialPowers.wrap(window).sessionStorage.getItem("X");
    ok(true, "getting sessionStorage didn't throw");
  } catch (e) {
    ok(false, "getting sessionStorage should not throw");
  }

  try {
    SpecialPowers.wrap(window).indexedDB;
    ok(true, "getting indexedDB didn't throw");
  } catch (e) {
    ok(false, "getting indexedDB should not throw");
  }

  try {
    var promise = SpecialPowers.wrap(window).caches.keys();
    ok(true, "getting caches didn't throw");

    return new Promise((resolve, reject) => {
      promise.then(function() {
        ok(location.protocol == "https:", "The promise was not rejected");
        resolve();
      }, function(e) {
        ok(location.protocol != "https:", "The promise should not have been rejected: " + e);
        resolve();
      });
    });
  } catch (e) {
    ok(false, "getting caches should not have thrown");
    return Promise.resolve();
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

  next_step();
}

var thirdparty = "https://example.com/tests/dom/tests/mochitest/general/";
