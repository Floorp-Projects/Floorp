/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const SOFT_START_HOSTNAME = "soft-start.example.invalid";

let MockDNSService = {
  RESOLVE_DISABLE_IPV6: 32,
  nowServing: 0,
  resultCode: 0,
  ipFirstOctet: 127,

  getNowServingAddress: function() {
    let ip = this.ipFirstOctet + "." +
             ((this.nowServing >>> 16) & 0xFF) + "." +
             ((this.nowServing >>> 8) & 0xFF) + "." +
             ((this.nowServing) & 0xFF);
    info("Using 'now serving' of " + this.nowServing + " (" + ip + ")");
    return ip;
  },

  asyncResolve: function(host, flags, callback) {
    let mds = this;
    Assert.equal(flags, this.RESOLVE_DISABLE_IPV6,
                 "AAAA lookup should be disabled");
    Assert.equal(host, SOFT_START_HOSTNAME,
                 "Configured hostname should be used");
    callback(null,
             {getNextAddrAsString: mds.getNowServingAddress.bind(mds)},
             this.resultCode);
  }
};

// We need an unfrozen copy of the LoopService so we can monkeypatch it.
let LoopService = {};
for (var prop in MozLoopService) {
  if (MozLoopService.hasOwnProperty(prop)) {
    LoopService[prop] = MozLoopService[prop];
  }
}
LoopService._DNSService = MockDNSService;

let runCheck = function(expectError) {
  return new Promise((resolve, reject) => {
    LoopService.checkSoftStart(error => {
      if ((!!error) != (!!expectError)) {
        reject(error);
      } else {
        resolve(error);
      }
    })
  });
}

add_task(function* test_mozLoop_softStart() {
  let orig_throttled = Services.prefs.getBoolPref("loop.throttled2");

  // Set associated variables to proper values
  Services.prefs.setBoolPref("loop.throttled2", true);
  Services.prefs.setCharPref("loop.soft_start_hostname", SOFT_START_HOSTNAME);
  Services.prefs.setIntPref("loop.soft_start_ticket_number", -1);

  registerCleanupFunction(function () {
    Services.prefs.setBoolPref("loop.throttled2", orig_throttled);
    Services.prefs.clearUserPref("loop.soft_start_ticket_number");
    Services.prefs.clearUserPref("loop.soft_start_hostname");
  });

  let throttled;
  let ticket;

  info("Ensure that we pick a valid ticket number.");
  yield runCheck();
  throttled = Services.prefs.getBoolPref("loop.throttled2");
  ticket = Services.prefs.getIntPref("loop.soft_start_ticket_number");
  Assert.equal(throttled, true, "Feature should still be throttled");
  Assert.notEqual(ticket, -1, "Ticket should be changed");
  Assert.ok((ticket < 16777214 && ticket > 0), "Ticket should be in range");

  // Try some "interesting" ticket numbers
  for (ticket of [1, 256, 65535, 10000000, 16777214]) {
    Services.prefs.setBoolPref("loop.throttled2", true);
    Services.prefs.setIntPref("loop.soft_start_ticket_number", ticket);

    info("Ensure that we don't activate when the now serving " +
         "number is less than our value.");
    MockDNSService.nowServing = ticket - 1;
    yield runCheck();
    throttled = Services.prefs.getBoolPref("loop.throttled2");
    Assert.equal(throttled, true, "Feature should still be throttled");

    info("Ensure that we don't activate when the now serving " +
         "number is equal to our value");
    MockDNSService.nowServing = ticket;
    yield runCheck();
    throttled = Services.prefs.getBoolPref("loop.throttled2");
    Assert.equal(throttled, true, "Feature should still be throttled");

    info("Ensure that we *do* activate when the now serving " +
         "number is greater than our value");
    MockDNSService.nowServing = ticket + 1;
    yield runCheck();
    throttled = Services.prefs.getBoolPref("loop.throttled2");
    Assert.equal(throttled, false, "Feature should be unthrottled");
  }

  info("Check DNS error behavior");
  MockDNSService.nowServing = 0;
  MockDNSService.resultCode = 0x80000000;
  Services.prefs.setBoolPref("loop.throttled2", true);
  yield runCheck(true);
  throttled = Services.prefs.getBoolPref("loop.throttled2");
  Assert.equal(throttled, true, "Feature should be throttled");

  info("Check DNS misconfiguration behavior");
  MockDNSService.nowServing = ticket + 1;
  MockDNSService.resultCode = 0;
  MockDNSService.ipFirstOctet = 6;
  Services.prefs.setBoolPref("loop.throttled2", true);
  yield runCheck(true);
  throttled = Services.prefs.getBoolPref("loop.throttled2");
  Assert.equal(throttled, true, "Feature should be throttled");
});
