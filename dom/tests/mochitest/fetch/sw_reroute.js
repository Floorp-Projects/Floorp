var gRegistration;

function testScript(script) {
  function setupSW(registration) {
    gRegistration = registration;

    var iframe = document.createElement("iframe");
    iframe.src = "reroute.html?" + script.replace(".js", "");
    document.body.appendChild(iframe);
  }

  SpecialPowers.pushPrefEnv({
    "set": [["dom.serviceWorkers.enabled", true],
            ["dom.serviceWorkers.testing.enabled", true],
            ["dom.serviceWorkers.exemptFromPerDomainMax", true]]
  }, function() {
    navigator.serviceWorker.ready.then(setupSW);
    navigator.serviceWorker.register("reroute.js", {scope: "/"});
  });
}

function finishTest() {
  gRegistration.unregister().then(SimpleTest.finish, function(e) {
    dump("unregistration failed: " + e + "\n");
    SimpleTest.finish();
  });
}

SimpleTest.waitForExplicitFinish();
