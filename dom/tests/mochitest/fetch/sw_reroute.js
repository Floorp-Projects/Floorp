var gRegistration;
var iframe;

function testScript(script) {
  var scope = "./reroute.html?" + script.replace(".js", "");
  function setupSW(registration) {
    gRegistration = registration;

    iframe = document.createElement("iframe");
    iframe.src = scope;
    document.body.appendChild(iframe);
  }

  SpecialPowers.pushPrefEnv({
    "set": [["dom.serviceWorkers.enabled", true],
            ["dom.serviceWorkers.testing.enabled", true],
            ["dom.serviceWorkers.exemptFromPerDomainMax", true]]
  }, function() {
    var scriptURL = location.href.includes("sw_empty_reroute.html")
                  ? "empty.js" : "reroute.js";
    navigator.serviceWorker.register(scriptURL, {scope: scope})
      .then(swr => waitForState(swr.installing, 'activated', swr))
      .then(setupSW);
  });
}

function finishTest() {
  iframe.remove();
  gRegistration.unregister().then(SimpleTest.finish, function(e) {
    dump("unregistration failed: " + e + "\n");
    SimpleTest.finish();
  });
}

SimpleTest.waitForExplicitFinish();
