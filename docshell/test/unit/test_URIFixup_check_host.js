/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

// Ensure DNS lookups don't hit the server
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDNSOverride",
  "@mozilla.org/network/native-dns-override;1",
  "nsINativeDNSResolverOverride"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDNSService",
  "@mozilla.org/network/dns-service;1",
  "nsIDNSService"
);

add_task(async function setup() {
  Services.prefs.setStringPref("browser.fixup.alternate.prefix", "www.");
  Services.prefs.setStringPref("browser.fixup.alternate.suffix", ".com");
  Services.prefs.setStringPref("browser.fixup.alternate.protocol", "https");
  Services.prefs.setBoolPref(
    "browser.urlbar.dnsResolveFullyQualifiedNames",
    true
  );
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.fixup.alternate.prefix");
    Services.prefs.clearUserPref("browser.fixup.alternate.suffix");
    Services.prefs.clearUserPref("browser.fixup.alternate.protocol");
    Services.prefs.clearUserPref(
      "browser.urlbar.dnsResolveFullyQualifiedNames"
    );
  });
});

// TODO: Export Listener from test_dns_override instead of duping
class Listener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }

  async firstAddress() {
    let all = await this.addresses();
    if (all.length) {
      return all[0];
    }
    return undefined;
  }

  async addresses() {
    let [, inRecord] = await this.promise;
    let addresses = [];
    if (!inRecord) {
      return addresses; // returns []
    }
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    while (inRecord.hasMore()) {
      addresses.push(inRecord.getNextAddrAsString());
    }
    return addresses;
  }

  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

const FAKE_IP = "::1";
const FAKE_INTRANET_IP = "::2";
const ORIGIN_ATTRIBUTE = {};

add_task(async function test_uri_with_force_fixup() {
  let listener = new Listener();
  let { fixedURI } = Services.uriFixup.forceHttpFixup("http://www.example.com");

  lazy.gDNSOverride.addIPOverride(fixedURI.displayHost, FAKE_IP);

  Services.uriFixup.checkHost(fixedURI, listener, ORIGIN_ATTRIBUTE);
  Assert.equal(
    await listener.firstAddress(),
    FAKE_IP,
    "Should've received fake IP"
  );

  lazy.gDNSOverride.clearHostOverride(fixedURI.displayHost);
  lazy.gDNSService.clearCache(false);
});

add_task(async function test_uri_with_get_fixup() {
  let listener = new Listener();
  let uri = Services.io.newURI("http://www.example.com");

  lazy.gDNSOverride.addIPOverride(uri.displayHost, FAKE_IP);

  Services.uriFixup.checkHost(uri, listener, ORIGIN_ATTRIBUTE);
  Assert.equal(
    await listener.firstAddress(),
    FAKE_IP,
    "Should've received fake IP"
  );

  lazy.gDNSOverride.clearHostOverride(uri.displayHost);
  lazy.gDNSService.clearCache(false);
});

add_task(async function test_intranet_like_uri() {
  let listener = new Listener();
  let uri = Services.io.newURI("http://someintranet");

  lazy.gDNSOverride.addIPOverride(uri.displayHost, FAKE_IP);
  // Hosts without periods should end with a period to make them FQN
  lazy.gDNSOverride.addIPOverride(uri.displayHost + ".", FAKE_INTRANET_IP);

  Services.uriFixup.checkHost(uri, listener, ORIGIN_ATTRIBUTE);
  Assert.deepEqual(
    await listener.addresses(),
    FAKE_INTRANET_IP,
    "Should've received fake intranet IP"
  );

  lazy.gDNSOverride.clearHostOverride(uri.displayHost);
  lazy.gDNSOverride.clearHostOverride(uri.displayHost + ".");
  lazy.gDNSService.clearCache(false);
});

add_task(async function test_intranet_like_uri_without_fixup() {
  let listener = new Listener();
  let uri = Services.io.newURI("http://someintranet");
  Services.prefs.setBoolPref(
    "browser.urlbar.dnsResolveFullyQualifiedNames",
    false
  );

  lazy.gDNSOverride.addIPOverride(uri.displayHost, FAKE_IP);
  // Hosts without periods should end with a period to make them FQN
  lazy.gDNSOverride.addIPOverride(uri.displayHost + ".", FAKE_INTRANET_IP);

  Services.uriFixup.checkHost(uri, listener, ORIGIN_ATTRIBUTE);
  Assert.deepEqual(
    await listener.addresses(),
    FAKE_IP,
    "Should've received non-fixed up IP"
  );

  lazy.gDNSOverride.clearHostOverride(uri.displayHost);
  lazy.gDNSOverride.clearHostOverride(uri.displayHost + ".");
  lazy.gDNSService.clearCache(false);
});

add_task(async function test_ip_address() {
  let listener = new Listener();
  let uri = Services.io.newURI("http://192.168.0.1");

  // Testing IP address being passed into the method
  // requires observing if the asyncResolve method gets called
  let didResolve = false;
  let topic = "uri-fixup-check-dns";
  let observer = {
    observe(aSubject, aTopicInner, aData) {
      if (aTopicInner == topic) {
        didResolve = true;
      }
    },
  };
  Services.obs.addObserver(observer, topic);
  lazy.gDNSOverride.addIPOverride(uri.displayHost, FAKE_IP);

  Services.uriFixup.checkHost(uri, listener, ORIGIN_ATTRIBUTE);
  Assert.equal(
    didResolve,
    false,
    "Passing an IP address should not conduct a host lookup"
  );

  lazy.gDNSOverride.clearHostOverride(uri.displayHost);
  lazy.gDNSService.clearCache(false);
  Services.obs.removeObserver(observer, topic);
});
