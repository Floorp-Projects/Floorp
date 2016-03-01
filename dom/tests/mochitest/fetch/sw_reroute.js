var gRegistration;

function testScript(script) {
  function setupSW(registration) {
    gRegistration = registration;

    var iframe = document.createElement("iframe");
    iframe.src = "reroute.html?" + script.replace(".js", "");
    document.body.appendChild(iframe);
  }

  SpecialPowers.pushPrefEnv({
    "set": [["dom.requestcache.enabled", true],
            ["dom.serviceWorkers.enabled", true],
            ["dom.serviceWorkers.testing.enabled", true],
            ["dom.serviceWorkers.exemptFromPerDomainMax", true]]
  }, function() {
    navigator.serviceWorker.ready.then(setupSW);
    var scriptURL = location.href.includes("sw_empty_reroute.html")
                  ? "empty.js" : "reroute.js";
    navigator.serviceWorker.register(scriptURL, {scope: "/"});
  });
}

function finishTest() {
  gRegistration.unregister().then(SimpleTest.finish, function(e) {
    dump("unregistration failed: " + e + "\n");
    SimpleTest.finish();
  });
}

SimpleTest.waitForExplicitFinish();
