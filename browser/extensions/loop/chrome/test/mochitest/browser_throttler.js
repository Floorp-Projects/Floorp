/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { LoopThrottler } = window;
const THROTTLE_HOSTNAME = "throttle.example.invalid";

let origChannel = Services.prefs.getCharPref("app.update.channel");
Services.prefs.setCharPref("test.throttler", THROTTLE_HOSTNAME);
let origDNS = LoopThrottler._dns;
registerCleanupFunction(() => {
  Services.prefs.setCharPref("app.update.channel", origChannel);
  LoopThrottler._dns = origDNS;
});

// Keep track of how many DNS resolve calls.
let gNumResolved = 0;
function resetResolved() {
  gNumResolved = 0;
}

// Set the same threshold for all channels or individually.
let gTestIP = "127.0.0.0";
function setThreshold(release, beta, nightly) {
  gTestIP = ["127", release, beta || release, nightly || release].join(".");
}
LoopThrottler._dns = {
  RESOLVE_DISABLE_IPV6: {},

  asyncResolve(host, flags, callback) {
    gNumResolved++;
    Assert.strictEqual(host, THROTTLE_HOSTNAME, "should be using test host");
    Assert.strictEqual(flags, this.RESOLVE_DISABLE_IPV6, "should set disable ipv6 flag");
    if (gTestIP === null) {
      callback({}, null, 1);
    }
    else {
      callback(null, {
        getNextAddrAsString() {
          return gTestIP;
        }
      }, 0);
    }
  }
};

function testThrottler() {
  return LoopThrottler.check("test").then(() => true, () => false);
}

// With a 0 threshold, no ticket will pass.
add_task(function* test_0_percent() {
  resetResolved();
  setThreshold(0);
  Services.prefs.setIntPref("test.ticket", -1);

  Assert.ok(!(yield testThrottler()), "don't trigger for 0%");
  Assert.notEqual(Services.prefs.getIntPref("test.ticket"), -1, "ticket should be updated");
  Assert.equal(gNumResolved, 1, "dns resolved once");
});

// Even with 0 threshold, special ticket skips DNS.
add_task(function* test_short_circuit() {
  resetResolved();
  setThreshold(0);
  Services.prefs.setIntPref("test.ticket", 255);

  Assert.ok(yield testThrottler(), "trigger for short circuit");
  Assert.equal(Services.prefs.getIntPref("test.ticket"), 255, "ticket should be unchanged");
  Assert.equal(gNumResolved, 0, "dns not used");
});

// No ticket will pass for failed dns.
add_task(function* test_failed_dns() {
  resetResolved();
  gTestIP = null;
  Services.prefs.setIntPref("test.ticket", -1);

  Assert.ok(!(yield testThrottler()), "don't trigger for failed dns");
  Assert.notEqual(Services.prefs.getIntPref("test.ticket"), -1, "ticket should be updated");
  Assert.equal(gNumResolved, 1, "dns resolved once");
});

// Even with max threshold, no ticket will pass for non-loopback entry.
add_task(function* test_non_loopback() {
  resetResolved();
  gTestIP = "192.255.255.255";
  Services.prefs.setIntPref("test.ticket", -1);

  Assert.ok(!(yield testThrottler()), "don't trigger for invalid loopback");
  Assert.notEqual(Services.prefs.getIntPref("test.ticket"), -1, "ticket should be updated");
  Assert.equal(gNumResolved, 1, "dns resolved once");
});

// With max threshold, any ticket will pass.
add_task(function* test_100_percent() {
  resetResolved();
  setThreshold(255);
  Services.prefs.setIntPref("test.ticket", -1);

  Assert.ok(yield testThrottler(), "trigger for 100%");
  Assert.equal(Services.prefs.getIntPref("test.ticket"), 255, "ticket should be set to max");
  Assert.equal(gNumResolved, 1, "dns resolved once");
});

// Check with tickets just over/match/under the threshold.
add_task(function* test_1_threshold() {
  setThreshold(1);

  resetResolved();
  Services.prefs.setIntPref("test.ticket", 2);
  Assert.ok(!(yield testThrottler()), "don't trigger for over");

  resetResolved();
  Services.prefs.setIntPref("test.ticket", 1);
  Assert.ok(!(yield testThrottler()), "don't trigger for match");

  resetResolved();
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(yield testThrottler(), "trigger for under");
});

// Check that only nightly channel is activated.
add_task(function* test_nightly() {
  setThreshold("0", "0", "1");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "nightly");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(yield testThrottler(), "trigger for nightly");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "beta");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for beta");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "release");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for release");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "other");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for other");
});

// Check that only beta channel is activated.
add_task(function* test_beta() {
  setThreshold("0", "1", "0");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "nightly");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for nightly");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "beta");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(yield testThrottler(), "trigger for beta");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "release");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for release");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "other");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for other");
});

// Check that only release channel is activated.
add_task(function* test_release() {
  setThreshold("1", "0", "0");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "nightly");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for nightly");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "beta");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(!(yield testThrottler()), "don't trigger for beta");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "release");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(yield testThrottler(), "trigger for release");

  resetResolved();
  Services.prefs.setCharPref("app.update.channel", "other");
  Services.prefs.setIntPref("test.ticket", 0);
  Assert.ok(yield testThrottler(), "trigger for other");
});
