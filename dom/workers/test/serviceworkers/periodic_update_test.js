var oldSWVersion, newSWVersion;
// This will be set by the test to the base directory for the test files.
var gPrefix;

function start() {
  const Cc = SpecialPowers.Cc;
  const Ci = SpecialPowers.Ci;

  function testVersion() {
    // Verify that the service worker has been correctly updated.
    testFrame(gPrefix + "periodic/frame.html").then(function(body) {
      newSWVersion = parseInt(body);
      is(newSWVersion, 2, "Expected correct new version");
      ok(newSWVersion > oldSWVersion,
         "The SW should be successfully updated, old: " + oldSWVersion +
         ", new: " + newSWVersion);
      unregisterSW().then(function() {
        SimpleTest.finish();
      });
    });
  }

  registerSW().then(function() {
    return testFrame(gPrefix + "periodic/frame.html").then(function(body) {
      oldSWVersion = parseInt(body);
      is(oldSWVersion, 1, "Expected correct old version");
    });
  }).then(function() {
    return testFrame(gPrefix + "periodic/wait_for_update.html");
  }).then(function() {
    return testVersion();
  });
}

function testFrame(src) {
  return new Promise(function(resolve, reject) {
    var iframe = document.createElement("iframe");
    iframe.src = src;
    window.onmessage = function(e) {
      if (e.data.status == "callback") {
        window.onmessage = null;
        var result = e.data.data;
        iframe.src = "about:blank";
        document.body.removeChild(iframe);
        iframe = null;
        SpecialPowers.exactGC(window, function() {
          resolve(result);
        });
      }
    };
    document.body.appendChild(iframe);
  });
}

function registerSW() {
  return testFrame(gPrefix + "periodic/register.html");
}

function unregisterSW() {
  return testFrame(gPrefix + "periodic/unregister.html");
}

function runTheTest() {
  SimpleTest.waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.interception.enabled", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
    ['dom.serviceWorkers.interception.enabled', true],
  ]}, function() {
    start();
  });
}
